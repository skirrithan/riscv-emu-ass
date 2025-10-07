#include "decoder/disassembler_driver.h"
#include "decoder/decoder.h"
#include "decoder/formatter.h"
#include "common/utils.h"

#include <vector>
#include <cstdint>
#include <iostream>

static bool toWordLE(const std::vector<uint8_t>& buf, size_t i, uint32_t& out) {
    if (i + 4 > buf.size()) return false;
    out = (uint32_t)buf[i]
        | ((uint32_t)buf[i+1] << 8)
        | ((uint32_t)buf[i+2] << 16)
        | ((uint32_t)buf[i+3] << 24);
    return true;
}

int disassembleFile(const std::string& inPath, bool show_pc, bool show_raw) {
    auto bytes = readBinaryFile(inPath);
    if (bytes.empty()) {
        std::cerr << "disasm: failed to read or empty file: " << inPath << "\n";
        return 1;
    }

    uint32_t pc = 0;
    for (size_t i = 0; i + 4 <= bytes.size(); i += 4, pc += 4) {
        uint32_t word = 0;
        if (!toWordLE(bytes, i, word)) break;

        try {
            Decoded d = decodeWord(word, pc);
            std::cout << formatDecoded(d, show_pc, show_raw) << "\n";
        } catch (const std::exception& ex) {
            // Still print address/word so you can see where decode failed
            std::cerr << std::hex;
            if (show_pc) std::cerr << std::setw(8) << std::setfill('0') << pc << ": ";
            if (show_raw) std::cerr << formatHex32(word) << "  ";
            std::cerr << "??  ; " << ex.what() << "\n";
        }
    }

    // If file size isn't a multiple of 4, warn.
    if (bytes.size() % 4 != 0) {
        std::cerr << "disasm: warning: trailing " << (bytes.size() % 4)
                  << " byte(s) ignored (binary not word-aligned)\n";
    }
    return 0;
}
