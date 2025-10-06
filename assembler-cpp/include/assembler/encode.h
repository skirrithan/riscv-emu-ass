// parsed instructions -> encode to 32 bit words and resolve labels in pass 2
#pragma once
#include "assembler/parser.h"
#include "symbols.h"
#include <vector>
#include <cstdint>

class Encoder {
public:
    Encoder(Program& prog, SymbolTable& sym);
    std::vector<uint32_t> assemble();
private:
    uint32_t encodeInstr(const AsmInstr& ins, uint32_t pc);
    Program& prog_;
    SymbolTable& sym_;
};
