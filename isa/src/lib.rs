#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize}

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
fn check_imm_range(v: i32, bits:u8) -> Result<(), IsaError {
    let min = -(1 << (bits - 1));
    let max = (1 << (bits -1)) - 1;
    if v >= min && v <= max { Ok(()) } else { Err(IsaError::ImmOutOfRange) }
}

#[inline]
fn mask(width: u32) -> u32 { (1ue2 << width) - 1} // mask(2) turns (200) -> (011) can be used w &  to produce lsb 

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
    | ((funct3 & 0x7) << 12) | (imm_4_0 << 7) | (opcode & 0x7f) as u32
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





