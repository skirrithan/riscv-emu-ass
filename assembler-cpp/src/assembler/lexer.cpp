#include "assembler/lexer.h"
#include <cctype>

static bool isIdentStart(char c){ return std::isalpha((unsigned char)c) || c=='_' || c=='.'; }
static bool isIdentCont (char c){ return std::isalnum((unsigned char)c) || c=='_' || c=='.'; }

Lexer::Lexer(const std::string& s):src_(s){} //constructor for the class Lexer

char Lexer::peek() const { return pos_ < src_.size() ? src_[pos_] : '\0'; }
char Lexer::get() { char c = peek(); if(c=='\n'){ line_++; } if(pos_ < src_.size()) pos_++; return c; }
bool Lexer::eof() const { return pos_ >= src_.size(); }

std::vector<Token> Lexer::tokenize() {
  toks_.clear();
  pos_ = 0;
  line_ = 1;
  while(!eof()){
    // skip spaces and comments
    char c = peek();
    if (c==' '||c=='\t'||c=='\r'){ get(); continue; }
    if (c=='#'){ while(!eof() && peek()!='\n') get(); continue; }
    if (c=='/' && pos_+1<src_.size() && src_[pos_+1]=='/') { while(!eof() && peek()!='\n') get(); continue; }

    unsigned l=line_;
    if (c=='\n'){ toks_.push_back({TokKind::Newline, "\n", 0, l}); get(); continue; }
    if (c==','){ toks_.push_back({TokKind::Comma, ",", 0, l}); get(); continue; }
    if (c==':'){ toks_.push_back({TokKind::Colon, ":", 0, l}); get(); continue; }
    if (c=='('){ toks_.push_back({TokKind::LParen, "(", 0, l}); get(); continue; }
    if (c==')'){ toks_.push_back({TokKind::RParen, ")", 0, l}); get(); continue; }
    if (c=='+' ){ toks_.push_back({TokKind::Plus,  "+", 0, l}); get(); continue; }
    if (c=='-' ){ toks_.push_back({TokKind::Minus, "-", 0, l}); get(); continue; }

    // number: dec or 0x...
    if (std::isdigit((unsigned char)c)) {
      std::string s; while(std::isxdigit((unsigned char)peek()) || (s=="0" && (peek()=='x'||peek()=='X'))) s.push_back(get());
      int64_t v = 0;
      if (s.size()>2 && s[0]=='0' && (s[1]=='x'||s[1]=='X')) v = (int64_t)std::stoll(s, nullptr, 16);
      else v = (int64_t)std::stoll(s, nullptr, 10);
      toks_.push_back({TokKind::Imm, s, v, l});
      continue;
    }

    // register xN
    if (c=='x' && pos_+1<src_.size() && std::isdigit((unsigned char)src_[pos_+1])) {
      std::string s; s.push_back(get());
      while(std::isdigit((unsigned char)peek())) s.push_back(get());
      int64_t idx = std::stoll(s.substr(1));
      toks_.push_back({TokKind::Reg, s, idx, l});
      continue;
    }

    // identifier / mnemonic / directive
    if (isIdentStart(c)) {
      std::string s; s.push_back(get());
      while(isIdentCont(peek())) s.push_back(get());
      toks_.push_back({TokKind::Ident, s, 0, l});
      continue;
    }

    // unknown char → skip
    get();
  }
  toks_.push_back({TokKind::End, "", 0, line_});
  return toks_;
}
