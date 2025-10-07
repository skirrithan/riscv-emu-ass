#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Minimal decoded instruction object
struct Decoded {
    uint32_t pc = 0;           // address of this instruction
    uint32_t word = 0;         // raw 32-bit encoding
    std::string mnemonic;      // e.g., "ADDI"
    std::vector<std::string> operands; // already formatted (e.g., "x1", "0(x2)", "0x10")
};

// Decode one 32-bit RV32I word at 'pc' into a Decoded struct.
// Throws std::runtime_error on unknown/invalid encodings.
Decoded decodeWord(uint32_t word, uint32_t pc);

// Convenience pretty-printer: "00000010: ADDI x1, x0, 5"
std::string formatDecoded(const Decoded& d, bool show_pc = true);
