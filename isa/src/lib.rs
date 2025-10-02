#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

pub type Reg = u8; //unsigned 8 bit integer alia Reg


// ISA errors for validation and decoding
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IsaError { //makes enum public
    BadReg,
    ImmOutOfRange,
    BadOpcode,
    BadFunct,
}

//minimal instruction enum 
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug,Clone,PartialEq,Eq)]
pub enum Instr {
    // R - type - reg -> reg ops
    Add   { rd: Reg, rs1: Reg, rs2: Reg },
    Sub   { rd: Reg, rs1: Reg, rs2: Reg },
    
    // I - type  register + imm ops
    Addi  { rd: Reg, rs1: Reg, imm: i32 },
    Jalr  { rd: Reg, rs1: Reg, imm: i32 },
    Lw    { rd: Reg, rs1: Reg, imm: i32 }, //load word
    
    // S - type store instructions
    Sw    { rs1: Reg, rs2: Reg, imm: i32 },
    
    // B - type conditional branches
    Beq   { rs1: Reg, rs2: Reg, imm: i32 },
    
    // U - type upper immediate instructions
    Lui   { rd: Reg, imm: i32 }, //load upper immediate
    Auipc { rd: Reg, imm: i32 }, //add upper immediate to pc
    
    // J - type
    Jal   { rd: Reg, imm: i32 }, // imm = PC - relative byte offset
}

// -- bitfield layout constants -- 
// binary literal from the RISC-V instruction encoding tables
const OP_OP:        u32 = 0b0110011; // R type format instructions
const OP_OP_IMM:    u32 = 0b0010011; // I (ALU Immediate)
const OP_LOAD:      u32 = 0b0000011; // I (Loads)
const OP_STORE:     u32 = 0b0100011; // S
const OP_BRANCH:    u32 = 0b1100011; // B
const OP_JALR:      u32 = 0b1100111; // I (JALR)
const OP_JAL:       u32 = 0b1101111; // J
const OP_LUI:       u32 = 0b0110111; // U
const OP_AUIPC:     u32 = 0b0010111; // U

// funct3 instr[14:12] exact instruction in each class
const F3_ADD_SUB:   u32 = 0b000;
const F3_ADDI:      u32 = 0b000;
const F3_BEQ:       u32 = 0b000;
const F3_LW:        u32 = 0b010;
const F3_SW:        u32 = 0b010;
const F3_JALR:      u32 = 0b000;

// funct7 
const F7_ADD:       u32 = 0b0000000;
const F7_SUB:       u32 = 0b0100000;

// --utilities--
#[inline] //inline this function , replace function call w actual code avoids function call overhead
fn check_reg(r: Reg) -> Result<(), IsaError> { //declares a fn named check_reg, r:Reg takes a register index u8 and returns IsaError
    if r < 32 { Ok(()) } else { Err(IsaError::BadReg) }
}

#[inline]
fn check_imm_range(v: i32, bits:u8) -> Result<(), IsaError> {
    let min = -(1 << (bits - 1));
    let max = (1 << (bits -1)) - 1;
    if v >= min && v <= max { Ok(()) } else { Err(IsaError::ImmOutOfRange) }
}

#[inline]
fn mask(width: u32) -> u32 { (1u32 << width) - 1} // mask(2) turns (200) -> (011) can be used w &  to produce lsb 

#[inline]
fn sign_extend_u32(v: u32, bits: u8) -> i32 {
    let shift = 32 - bits as u32; // 'bits -12' , shift = 20, 12 bit number shift by 20, bit 11 is now 31
    ((v << shift) as i32) >> shift //shift left , sign 32 bit, shift back
}

// -- packers (R/I/S/B/U/J) --

#[inline]
fn pack_r(rd: Reg, rs1: Reg, rs2: Reg, funct3: u32, funct7: u32, opcode: u32) -> u32 {
    (((funct7 & 0x7f) << 25) | (((rs2 as u32) & 0x1f) << 20) | (((rs1 as u32) & 0x1f) << 15)
    | ((funct3 & 0x7) << 12) | (((rd as u32) & 0x1f) << 7) | (opcode & 0x7f)) as u32
}

#[inline]
fn pack_i(rd: Reg, rs1: Reg, imm12: i32, funct3: u32, opcode: u32) -> u32 {
    let imm = (imm12 as u32) & mask(12); 
    ((imm << 20) | (((rs1 as u32) & 0x1f) << 15) | ((funct3 & 0x7) << 12) 
    | (((rd as u32) & 0x1f) << 7) | (opcode & 0x7f)) as u32
}

#[inline]
fn pack_s(rs1: Reg, rs2: Reg, imm12: i32, funct3: u32, opcode: u32) -> u32 {
    let imm      = (imm12 as u32) & mask(12);
    let imm_11_5 = (imm >> 5) & 0x7f;
    let imm_4_0  = imm & 0x1f; 
    ((imm_11_5 << 25) | (((rs2 as u32) & 0x1f) << 20) | (((rs1 as u32) & 0x1f) << 15) 
    | ((funct3 & 0x7) << 12) | (imm_4_0 << 7) | (opcode & 0x7f)) as u32
}

#[inline]
fn pack_b(rs1: Reg, rs2: Reg, imm13: i32, funct3: u32, opcode: u32) -> u32 {
    let imm   = (imm13 as u32) & mask(13);
    let b12   = (imm >> 12) & 0x1;
    let b10_5 = (imm >> 5) & 0x3f;
    let b4_1  = (imm >> 1) & 0x0f;
    let b11   = (imm >> 11) & 0x1;
    ((b12<<31) | (b10_5 << 25) | (((rs2 as u32) & 0x1f) << 20) | (((rs1 as u32) & 0x1f) << 15) 
    | ((funct3 & 0x7) << 12) | (b4_1 << 8) | (b11 << 7) | (opcode & 0x7f)) as u32
}

#[inline]
fn pack_u(rd: Reg, imm20: i32, opcode: u32) -> u32 {
    let imm = ((imm20 as u32) & mask(20)) << 12; // imm occupies bits 31:12 (lower 12 bits zeroed by construction)
    (imm | (((rd as u32) & 0x1f) << 7) | (opcode & 0x7f)) as u32
}

fn pack_j(rd: Reg, imm21: i32, opcode: u32) -> u32 {
    let imm    = (imm21 as u32) & mask(21); //imm is a 21 bit signed 
    let b20    = (imm >> 20) & 0x1;
    let b10_1  = (imm >> 1) & 0x3ff;
    let b11    = (imm >> 11) & 0x1;
    let b19_12 = (imm >> 12) & 0xff;
    ((b20 << 31) | (b19_12 << 12) | (b11 << 20) | (b10_1 << 21) | (((rd as u32) & 0x1f) << 7) | (opcode & 0x7f)) as u32
}

// -- public encoders --

pub fn encode_add( rd: Reg, rs1: Reg, rs2: Reg ) -> Result<u32, IsaError> {
    check_reg(rd)?; check_reg(rs1)?; check_reg(rs2)?;
    Ok(pack_r( rd, rs1, rs2, F3_ADD_SUB, F7_ADD, OP_OP ))
}

pub fn encode_sub( rd: Reg, rs1: Reg, rs2: Reg ) -> Result<u32, IsaError> {
    check_reg(rd)?; check_reg(rs1)?; check_reg(rs2)?;
    Ok(pack_r( rd, rs1, rs2, F3_ADD_SUB, F7_SUB, OP_OP ))
}

pub fn encode_addi( rd: Reg, rs1: Reg, imm: i32 ) -> Result<u32, IsaError> {
    check_reg(rd)?; check_reg(rs1)?; check_imm_range(imm,12)?;
    Ok(pack_i( rd, rs1, imm, F3_ADDI, OP_OP_IMM ))
}

pub fn encode_lw( rd: Reg, rs1: Reg, imm: i32 ) -> Result<u32, IsaError> {
    check_reg(rd)?; check_reg(rs1)?; check_imm_range(imm,12)?;
    Ok(pack_i( rd, rs1, imm, F3_LW, OP_LOAD ))
}

pub fn encode_sw( rs1: Reg, rs2: Reg, imm: i32 ) -> Result<u32, IsaError> {
    check_reg(rs1)?; check_reg(rs2)?; check_imm_range(imm, 12)?;
    Ok(pack_s( rs1, rs2, imm, F3_SW, OP_STORE ))
}

//branch offset (bytes relative to pc ~ must be even)
pub fn encode_beq( rs1: Reg, rs2: Reg, imm: i32 ) -> Result<u32, IsaError> {
    check_reg(rs1)?; check_reg(rs2)?; 
    if (imm & 0x1) != 0 { return Err(IsaError::ImmOutOfRange); } //mask for the lsb, branch offsets must be multiples of 2 , 32 bit instructions , 4 bytes
    check_imm_range(imm, 13)?;
    Ok(pack_b( rs1, rs2, imm, F3_BEQ, OP_BRANCH ))
}

pub fn encode_jal(rd: Reg, imm: i32) -> Result<u32, IsaError> {
    check_reg(rd)?;
    if (imm & 0x1) != 0 { return Err(IsaError::ImmOutOfRange); }
    check_imm_range(imm, 21)?;
    Ok(pack_j(rd, imm, OP_JAL))
}

pub fn encode_jalr( rd: Reg, rs1: Reg, imm: i32 ) -> Result<u32, IsaError> {
    check_reg(rd)?; check_reg(rs1)?; check_imm_range(imm, 12)?;
    Ok(pack_i( rd, rs1, imm, F3_JALR, OP_JALR ))
}

pub fn encode_lui( rd: Reg, imm: i32 ) -> Result<u32, IsaError> {
    check_reg(rd)?; check_imm_range(imm, 20)?;
    Ok(pack_u( rd, imm, OP_LUI ))
}

pub fn encode_auipc( rd: Reg, imm: i32 ) -> Result<u32, IsaError> {
    check_reg(rd)?; check_imm_range(imm, 20)?;
    Ok(pack_u(rd, imm, OP_AUIPC))
}

//-- field extractors --
#[inline] pub fn rd(word: u32) -> Reg { ((word >> 7) & 0x1f) as Reg }
#[inline] pub fn funct3(word: u32) -> u32 { (word >> 12) & 0x7 }
#[inline] pub fn rs1(word: u32) -> Reg { ((word >> 15) & 0x1f) as Reg }
#[inline] pub fn rs2(word: u32) -> Reg { ((word >> 20) & 0x1f) as Reg }
#[inline] pub fn funct7(word: u32) -> u32 { (word >> 25) & 0x7f }
#[inline] pub fn opcode(word: u32) -> u32 { word & 0x7f }

#[inline]
pub fn imm_i(word: u32) -> i32 { sign_extend_u32(word >> 20, 12) }

#[inline]
pub fn imm_s(word: u32) -> i32 {
    let hi = (word >> 25) & 0x7f;
    let lo = (word >> 7) & 0x1f;
    let imm = (hi << 5) | lo;
    sign_extend_u32(imm, 12)
}

#[inline]
pub fn imm_b(word: u32) -> i32 {
    // [31|30:25|11:8|7] -> [12|10:5|4:1|11] plus low zero bit
    let b12   = ((word >> 31) & 0x1) << 12;
    let b10_5 = ((word >> 25) & 0x3f) << 5;
    let b4_1  = ((word >> 8)  & 0x0f) << 1;
    let b11   = ((word >> 7)  & 0x1) << 11;
    let imm = b12 | b11 | b10_5 | b4_1;
    sign_extend_u32(imm, 13)
}

#[inline]
pub fn imm_u(word: u32) -> i32 { (word & 0xfffff000) as i32 } // already aligned

#[inline]
pub fn imm_j(word: u32) -> i32 {
    // [31|30:21|20|19:12] -> [20|10:1|11|19:12]
    let b20   = ((word >> 31) & 0x1) << 20;
    let b10_1 = ((word >> 21) & 0x3ff) << 1;
    let b11   = ((word >> 20) & 0x1) << 11;
    let b19_12= ((word >> 12) & 0xff) << 12;
    let imm = b20 | b19_12 | b11 | b10_1;
    sign_extend_u32(imm, 21)
}

// -- minimal decoder --
pub fn decode( word: u32 ) -> Result<Instr, IsaError> {
    match opcode(word) {
        OP_OP => { 
            match ( funct3(word), funct7(word) ) {
                (F3_ADD_SUB, F7_ADD) => Ok(Instr::Add { rd: rd(word), rs1: rs1(word), rs2: rs2(word) }),
                (F3_ADD_SUB, F7_SUB) => Ok(Instr::Sub { rd: rd(word), rs1: rs1(word), rs2: rs2(word) }),
                _ => Err(IsaError::BadFunct),
            }
        }
        
        OP_OP_IMM => {
            match funct3(word) {
                F3_ADDI => Ok(Instr::Addi { rd: rd(word), rs1: rs1(word), imm: imm_i(word) }),
                _ => Err(IsaError::BadFunct),
            }
        }
        
        OP_LOAD => {
            match funct3(word) {
                F3_LW => Ok(Instr::Lw { rd: rd(word), rs1: rs1(word), imm: imm_i(word) }),
                _ => Err(IsaError::BadFunct),
            }
        }
        
        OP_STORE => {
            match funct3(word) {
                F3_SW => Ok(Instr::Sw { rs1: rs1(word), rs2: rs2(word), imm: imm_s(word) }),
                _ => Err(IsaError::BadFunct),
            }
        }
        
        OP_BRANCH => {
            match funct3(word) {
                F3_BEQ => Ok(Instr::Beq { rs1: rs1(word), rs2: rs2(word), imm: imm_b(word) }),
                _ => Err(IsaError::BadFunct),
            }
        }
        
        OP_JAL => Ok(Instr::Jal { rd: rd(word), imm: imm_j(word) }),
        
        OP_JALR => {
            if funct3(word) == F3_JALR {
                Ok(Instr::Jalr { rd: rd(word), rs1: rs1(word), imm: imm_i(word) })
            } 
            else { Err(IsaError::BadFunct) }
        }
        
        OP_LUI => Ok(Instr::Lui { rd: rd(word), imm: imm_u(word) }),
        
        OP_AUIPC => Ok(Instr::Auipc { rd: rd(word), imm: imm_u(word) }),
        
        _ => Err(IsaError::BadOpcode),
    }
}

// -- tests --
#[cfg(feature = "std")]
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn add_round_trip() {
        let w = encode_add(1,2,3).unwrap();
        println!("Encoded Add instruction: {:#034b}", w);
        assert_eq!(decode(w).unwrap(), Instr::Add { rd:1, rs1:2, rs2:3 });
    }
    
    #[test]
    fn addi_i_imm_limits() {
        assert!(encode_addi(1,2, 2047).is_ok());
        assert!(encode_addi(1, 2, -2048).is_ok());
        assert!(encode_addi(1, 2, 2048).is_err());
    }
    
    #[test]
    fn beq_b_imm_even() {
        // valid even offset
        assert!(encode_beq(1,2,8).is_ok());
        // odd (not multiple of 2) → error
        assert!(encode_beq(1, 2, 3).is_err());
    }

    #[test]
    fn jal_round_trip() {
        let w = encode_jal(1, -4).unwrap();
        assert_eq!(decode(w).unwrap(), Instr::Jal { rd:1, imm:-4 });
    }
}