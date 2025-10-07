#include "decoder/decoder.h"
#include "common/utils.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cctype>

static std::string regName(uint8_t r) {
    return "x" + std::to_string((unsigned)r);
}

static std::string hex32(uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

static int32_t imm_i(uint32_t w) { return (int32_t)w >> 20; } // sign-extended 12-bit
static int32_t imm_s(uint32_t w) {
    uint32_t i = (bits(w, 31, 25) << 5) | bits(w, 11, 7);
    return signExtend(i, 12);
}
static int32_t imm_b(uint32_t w) {
    // B-type split: imm[12|10:5|4:1|11] << {31,30:25,11:8,7}
    uint32_t b12   = bits(w, 31, 31);
    uint32_t b10_5 = bits(w, 30, 25);
    uint32_t b4_1  = bits(w, 11, 8);
    uint32_t b11   = bits(w, 7, 7);
    uint32_t i = (b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1);
    return signExtend(i, 13);
}
static int32_t imm_u(uint32_t w) { return (int32_t)(w & 0xFFFFF000u); } // upper 20 bits, low 12 zeros
static int32_t imm_j(uint32_t w) {
    // J-type split: imm[20|10:1|11|19:12] << {31,30:21,20,19:12}
    uint32_t j20   = bits(w, 31, 31);
    uint32_t j10_1 = bits(w, 30, 21);
    uint32_t j11   = bits(w, 20, 20);
    uint32_t j19_12= bits(w, 19, 12);
    uint32_t i = (j20 << 20) | (j19_12 << 12) | (j11 << 11) | (j10_1 << 1);
    return signExtend(i, 21);
}

Decoded decodeWord(uint32_t w, uint32_t pc) {
    Decoded d;
    d.pc = pc;
    d.word = w;

    uint32_t opcode = bits(w, 6, 0);
    uint8_t  rd     = (uint8_t)bits(w, 11, 7);
    uint8_t  funct3 = (uint8_t)bits(w, 14, 12);
    uint8_t  rs1    = (uint8_t)bits(w, 19, 15);
    uint8_t  rs2    = (uint8_t)bits(w, 24, 20);
    uint8_t  funct7 = (uint8_t)bits(w, 31, 25);

    switch (opcode) {
    // ---------- R-type: ADD/SUB ----------
    case 0x33: {
        if (funct3 == 0x0 && funct7 == 0x00) {
            d.mnemonic = "ADD";
            d.operands = { regName(rd), regName(rs1), regName(rs2) };
            return d;
        }
        if (funct3 == 0x0 && funct7 == 0x20) {
            d.mnemonic = "SUB";
            d.operands = { regName(rd), regName(rs1), regName(rs2) };
            return d;
        }
        break;
    }
    // ---------- I-type arithmetic: ADDI ----------
    case 0x13: {
        if (funct3 == 0x0) { // ADDI
            int32_t imm = imm_i(w);
            d.mnemonic = "ADDI";
            d.operands = { regName(rd), regName(rs1), std::to_string(imm) };
            return d;
        }
        break;
    }
    // ---------- I-type loads: LW ----------
    case 0x03: {
        if (funct3 == 0x2) { // LW
            int32_t imm = imm_i(w);
            d.mnemonic = "LW";
            std::ostringstream mem; mem << imm << "(" << regName(rs1) << ")";
            d.operands = { regName(rd), mem.str() };
            return d;
        }
        break;
    }
    // ---------- S-type stores: SW ----------
    case 0x23: {
        if (funct3 == 0x2) { // SW
            int32_t imm = imm_s(w);
            d.mnemonic = "SW";
            std::ostringstream mem; mem << imm << "(" << regName(rs1) << ")";
            d.operands = { regName(rs2), mem.str() };
            return d;
        }
        break;
    }
    // ---------- B-type branches: BEQ ----------
    case 0x63: {
        if (funct3 == 0x0) { // BEQ
            int32_t off = imm_b(w);      // byte offset (signed)
            uint32_t tgt = pc + off;     // PC-relative target
            d.mnemonic = "BEQ";
            d.operands = { regName(rs1), regName(rs2), hex32(tgt) };
            return d;
        }
        break;
    }
    // ---------- U-type: LUI ----------
    case 0x37: {
        int32_t imm = imm_u(w);
        d.mnemonic = "LUI";
        d.operands = { regName(rd), hex32((uint32_t)imm) };
        return d;
    }
    // ---------- U-type: AUIPC ----------
    case 0x17: {
        int32_t imm = imm_u(w);
        d.mnemonic = "AUIPC";
        d.operands = { regName(rd), hex32((uint32_t)imm) };
        return d;
    }
    // ---------- J-type: JAL ----------
    case 0x6F: {
        int32_t off = imm_j(w);
        uint32_t tgt = pc + off;
        d.mnemonic = "JAL";
        d.operands = { regName(rd), hex32(tgt) };
        return d;
    }
    // ---------- I-type jumps: JALR ----------
    case 0x67: {
        if (funct3 == 0x0) {
            int32_t imm = imm_i(w);
            d.mnemonic = "JALR";
            d.operands = { regName(rd), regName(rs1), std::to_string(imm) };
            return d;
        }
        break;
    }
    default:
        break;
    }

    // If we reach here: unknown/unsupported encoding
    std::ostringstream oss;
    oss << "unknown encoding: opcode=0x" << std::hex << (unsigned)opcode
        << " word=" << hex32(w);
    throw std::runtime_error(oss.str());
}

std::string formatDecoded(const Decoded& d, bool show_pc) {
    std::ostringstream oss;
    if (show_pc) {
        oss << std::hex << std::setw(8) << std::setfill('0') << d.pc << ": ";
    }
    oss << d.mnemonic;
    if (!d.operands.empty()) {
        oss << " ";
        for (size_t i = 0; i < d.operands.size(); ++i) {
            if (i) oss << ", ";
            oss << d.operands[i];
        }
    }
    return oss.str();
}
