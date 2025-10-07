#include "decoder/formatter.h"
#include <sstream>
#include <iomanip>

std::string formatHex32(uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

std::string formatPc(uint32_t pc) {
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << pc;
    return oss.str();
}

std::string formatDecoded(const Decoded& d, bool show_pc, bool show_raw) {
    std::ostringstream oss;

    if (show_pc)  oss << formatPc(d.pc) << ": ";
    if (show_raw) oss << formatHex32(d.word) << "  ";

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
