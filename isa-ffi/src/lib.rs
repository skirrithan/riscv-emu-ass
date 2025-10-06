#![cfg_attr(not(feature = "std"), no_std)]
#![allow(unreachable_patterns)]
#![allow(non_camel_case_types)]

use core::ffi::c_char;
use isa::{self as rv, Instr, IsaError};

//status codes returned from FFI calls
#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum IsaStatus {
    ISA_OK = 0,
    ISA_BAD_REG = 1,
    ISA_IMM_OUT_OF_RANGE = 2,
    ISA_BAD_OPCODE = 3,
    ISA_BAD_FUNCT = 4,
    ISA_NULL_PTR = 5,
    ISA_UNKNOWN = 255,
}

#[inline]
fn map_err(e: IsaError) -> IsaStatus {
    match e {
        IsaError::BadReg        => IsaStatus::ISA_BAD_REG,
        IsaError::ImmOutOfRange => IsaStatus::ISA_IMM_OUT_OF_RANGE,
        IsaError::BadOpcode     => IsaStatus::ISA_BAD_OPCODE,
        IsaError::BadFunct      => IsaStatus::ISA_BAD_FUNCT,
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum InstrTag {
    TAG_INVALID = 0,
    
    //R-type
    TAG_ADD,
    TAG_SUB,
    
    //I-type
    TAG_ADDI,
    TAG_JALR,
    TAG_LW,
    
    //S-type
    TAG_SW,
    
    //B-type
    TAG_BEQ,
    
    //U-type
    TAG_LUI,
    TAG_AUIPC,
    
    //J-type
    TAG_JAL,
}

//C-friendly  decoded instruction record
#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct DecodedInstr {
    pub tag: InstrTag,
    pub rd: u8,
    pub rs1: u8,
    pub rs2: u8,
    pub imm: i32,
}

//--small helpers--

#[inline]
fn out_ptr<T>(p: *mut T) -> Result<&'static mut T, IsaStatus> {
    if p.is_null() { Err(IsaStatus::ISA_NULL_PTR) } else { Ok(unsafe { &mut *p }) }
}

// -- optional status --

const STR_OK:       &[u8] = b"OK\0";
const STR_BADREG:   &[u8] = b"BadReg\0";
const STR_IMM:      &[u8] = b"ImmOutOfRange\0";
const STR_OPCODE:   &[u8] = b"BadOpcode\0";
const STR_FUNCT:    &[u8] = b"BadFunct\0";
const STR_NULL:     &[u8] = b"NullPtr\0";
const STR_UNKNOWN:  &[u8] = b"Unknown\0";

#[no_mangle]
pub extern "C" fn isa_status_str(code: IsaStatus) -> *const c_char {
    match code {
        IsaStatus::ISA_OK                   => STR_OK,
        IsaStatus::ISA_BAD_REG              => STR_BADREG,
        IsaStatus::ISA_IMM_OUT_OF_RANGE     => STR_IMM,
        IsaStatus::ISA_BAD_OPCODE           => STR_OPCODE,
        IsaStatus::ISA_BAD_FUNCT            => STR_FUNCT,
        IsaStatus::ISA_NULL_PTR             => STR_NULL,
        IsaStatus::ISA_UNKNOWN              => STR_UNKNOWN,
        _                                   => STR_UNKNOWN,
    }.as_ptr() as *const c_char
}

//-- encoders --
#[no_mangle]
pub extern "C" fn isa_encode_add(rd: u8, rs1: u8, rs2: u8, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_add(rd, rs1, rs2) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_sub(rd: u8, rs1: u8, rs2: u8, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_sub(rd, rs1, rs2) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_addi(rd: u8, rs1: u8, imm: i32, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_addi(rd, rs1, imm) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_lw(rd: u8, rs1: u8, imm: i32, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_lw(rd, rs1, imm) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_sw(rs1: u8, rs2: u8, imm: i32, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_sw(rs1, rs2, imm) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_beq(rs1: u8, rs2: u8, imm: i32, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_beq(rs1, rs2, imm) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_jal(rd: u8, imm: i32, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_jal(rd, imm) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_jalr(rd: u8, rs1: u8, imm: i32, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_jalr(rd, rs1, imm) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_lui(rd: u8, imm20: i32, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_lui(rd, imm20) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

#[no_mangle]
pub extern "C" fn isa_encode_auipc(rd: u8, imm20: i32, out_word: *mut u32) -> IsaStatus {
    let out = match out_ptr(out_word) { Ok(p) => p, Err(e) => return e };
    match rv::encode_auipc(rd, imm20) {
        Ok(w) => { *out = w; IsaStatus::ISA_OK }
        Err(e) => map_err(e),
    }
}

// -- decoder --

#[no_mangle]
pub extern "C" fn isa_decode(word: u32, out_decoded: *mut DecodedInstr) -> IsaStatus {
    let out = match out_ptr(out_decoded) { Ok(p) => p, Err(e) => return e };
    match rv::decode(word) {
        Ok(instr) => {
            *out = match instr {
                Instr::Add  { rd, rs1, rs2 } => DecodedInstr { tag: InstrTag::TAG_ADD,  rd, rs1, rs2, imm: 0 },
                Instr::Sub  { rd, rs1, rs2 } => DecodedInstr { tag: InstrTag::TAG_SUB,  rd, rs1, rs2, imm: 0 },
                Instr::Addi { rd, rs1, imm } => DecodedInstr { tag: InstrTag::TAG_ADDI, rd, rs1, rs2: 0, imm },
                Instr::Jalr { rd, rs1, imm } => DecodedInstr { tag: InstrTag::TAG_JALR, rd, rs1, rs2: 0, imm },
                Instr::Lw   { rd, rs1, imm } => DecodedInstr { tag: InstrTag::TAG_LW,   rd, rs1, rs2: 0, imm },
                Instr::Sw   { rs1, rs2, imm }=> DecodedInstr { tag: InstrTag::TAG_SW,   rd: 0, rs1, rs2, imm },
                Instr::Beq  { rs1, rs2, imm }=> DecodedInstr { tag: InstrTag::TAG_BEQ,  rd: 0, rs1, rs2, imm },
                Instr::Lui  { rd, imm }      => DecodedInstr { tag: InstrTag::TAG_LUI,  rd, rs1: 0, rs2: 0, imm },
                Instr::Auipc{ rd, imm }      => DecodedInstr { tag: InstrTag::TAG_AUIPC,rd, rs1: 0, rs2: 0, imm },
                Instr::Jal  { rd, imm }      => DecodedInstr { tag: InstrTag::TAG_JAL,  rd, rs1: 0, rs2: 0, imm },
            };
            IsaStatus::ISA_OK
        }
        Err(e) => map_err(e),
    }
}

// -- optional imm extractors --
#[no_mangle] pub extern "C" fn isa_field_rd(word: u32)     -> u8  { rv::rd(word) }
#[no_mangle] pub extern "C" fn isa_field_rs1(word: u32)    -> u8  { rv::rs1(word) }
#[no_mangle] pub extern "C" fn isa_field_rs2(word: u32)    -> u8  { rv::rs2(word) }
#[no_mangle] pub extern "C" fn isa_field_opcode(word: u32) -> u32 { rv::opcode(word) }
#[no_mangle] pub extern "C" fn isa_field_funct3(word: u32) -> u32 { rv::funct3(word) }
#[no_mangle] pub extern "C" fn isa_field_funct7(word: u32) -> u32 { rv::funct7(word) }

#[no_mangle] pub extern "C" fn isa_imm_i(word: u32) -> i32 { rv::imm_i(word) }
#[no_mangle] pub extern "C" fn isa_imm_s(word: u32) -> i32 { rv::imm_s(word) }
#[no_mangle] pub extern "C" fn isa_imm_b(word: u32) -> i32 { rv::imm_b(word) }
#[no_mangle] pub extern "C" fn isa_imm_u(word: u32) -> i32 { rv::imm_u(word) }
#[no_mangle] pub extern "C" fn isa_imm_j(word: u32) -> i32 { rv::imm_j(word) }

// -- ABI Version --
#[no_mangle]
pub extern "C" fn isa_ffi_version() -> u32 { 1 }