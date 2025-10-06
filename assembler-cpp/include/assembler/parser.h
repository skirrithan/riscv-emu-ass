// turn tokens into lightweight AST (abstract syntax tree) / list of instruction lines and label defs
#pragma once
#include "assembler/lexer.h"
#include <string>
#include <vector>

struct AsmInstr {
    std::string mnemonic;
    std::vector<std::string> args;
    unsigned line;
};

struct LabelDef {
    std::string name;
    unsigned line;
};

struct Program {
    std::vector<LabelDef> labels;
    std::vector<AsmInstr> instrs;
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& toks);
    Program parse();
private:
    const Token& peek(int k = 0) const;
    bool accept(TokKind k);
    bool expect(TokKind k);
    std::vector<Token> toks_;
    size_t i_ = 0;
};
