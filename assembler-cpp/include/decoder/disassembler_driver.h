#pragma once
#include <string>

// Disassemble a raw binary (little-endian 32-bit RISC-V words) and print to stdout.
// Returns 0 on success, non-zero on error.
//
// Options:
//  - show_pc : prefix each line with the PC (addr)
//  - show_raw: also show the raw 32-bit word before the mnemonic
int disassembleFile(const std::string& inPath, bool show_pc = true, bool show_raw = false);
