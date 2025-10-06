#pragma once
#include <string>
#include <vector>
#include <cstdint>

//
// Utility functions shared between assembler and disassembler
//

// === File helpers ===

// Reads entire file contents into a string (for .s files)
std::string readFileToString(const std::string& path);

// Reads entire binary file into vector<uint8_t> (for disassembler)
std::vector<uint8_t> readBinaryFile(const std::string& path);

// Writes a vector<uint8_t> to a binary file
bool writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data);

// Writes a vector<uint32_t> as 32-bit little-endian words
bool writeBinaryWords(const std::string& path, const std::vector<uint32_t>& words);

// === Bit manipulation helpers ===

// Extract bits from value (inclusive range)
inline uint32_t bits(uint32_t value, int hi, int lo) {
    return (value >> lo) & ((1u << (hi - lo + 1)) - 1);
}

// Sign-extend an n-bit immediate (e.g., signExtend(imm, 12))
inline int32_t signExtend(uint32_t value, int bits) {
    int32_t shift = 32 - bits;
    return (int32_t)(value << shift) >> shift;
}

// Align value up to next multiple of 'align'
inline uint32_t alignUp(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

