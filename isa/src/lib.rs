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