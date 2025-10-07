#pragma once
#include "decoder/decoder.h"
#include <string>

// "00000010: 0x002081b3  ADD x3, x1, x2"
// If show_raw is true, includes the raw word after the PC.
std::string formatDecoded(const Decoded& d, bool show_pc = true, bool show_raw = false);

// Helpers if you ever want to reuse:
std::string formatHex32(uint32_t v); // "0xXXXXXXXX"
std::string formatPc(uint32_t pc);   // "XXXXXXXX"
