//purpose: read raw text → produce tokens (LABEL, MNEMONIC, REGISTER, IMMEDIATE, COMMA, etc.)
#pragma once
#include <string>
#include <vector>

enum class TokKind { Ident, Reg, Imm, Comma, Colon, Newline, End };

struct Token {
    TokKind kind;
    std::string text;
    int64_t value = 0;
    unsigned line = 0;
};

class Lexer {
public:
    explicit Lexer(const std::string& src);
    std::vector<Token> tokenize();
private:
    char peek() const;
    char get();
    bool eof() const;

    const std::string& src_;
    size_t pos_ = 0;
    unsigned line_ = 1;
};
