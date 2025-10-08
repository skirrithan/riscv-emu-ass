#include "assembler/parser.h"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& t):toks_(t){}

const Token& Parser::peek(int k) const {
  size_t j = i_ + (size_t)k;
  if (j >= toks_.size()) return toks_.back();
  return toks_[j];
}
bool Parser::accept(TokKind k){ if(peek().kind==k){ i_++; return true; } return false; }
bool Parser::expect(TokKind k, const char* msg){
  if (accept(k)) return true;
  errs_.push_back(std::string("parse error (line ")+std::to_string(peek().line)+"): expected "+msg);
  return false;
}

Program Parser::parse(){
  Program P;
  while (peek().kind != TokKind::End){
    auto L = parseLine();
    if (!L.labels.empty()) for (auto& nm: L.labels) P.labels.push_back({nm, L.line});
    if (!L.mnemonic.empty()) P.instrs.push_back({L.mnemonic, L.operands, L.line});
    while (accept(TokKind::Newline)) {}
  }
  return P;
}

Line Parser::parseLine(){
  Line L; L.line = peek().line;
  // labels prefix: ident ':'
  while (peek().kind==TokKind::Ident && peek(1).kind==TokKind::Colon){
    L.labels.push_back(peek().text);
    i_+=2; // consume ident+colon
    while (accept(TokKind::Newline)) {} // label-alone line ok
  }
  if (peek().kind==TokKind::Ident){
    L.mnemonic = peek().text;
    i_++;
    // operands: a, b, c | allow forms like:  imm, x1, label, 12(x2)
    while (true){
      if (peek().kind==TokKind::Newline || peek().kind==TokKind::End) break;
      // capture a token sequence until comma/newline
      std::string op;
      // allow patterns like 12(x5) or -8(x2)
      int par = 0;
      while (true) {
        auto k = peek().kind;
        if (k==TokKind::Comma && par==0) break;
        if (k==TokKind::Newline || k==TokKind::End) break;
        op += peek().text;
        if (k==TokKind::LParen) par++;
        if (k==TokKind::RParen && par>0) par--;
        i_++;
      }
      // trim spaces
      while(!op.empty() && (op.back()==' '||op.back()=='\t')) op.pop_back();
      while(!op.empty() && (op.front()==' '||op.front()=='\t')) op.erase(op.begin());
      if(!op.empty()) L.operands.push_back(op);
      accept(TokKind::Comma);
    }
  }
  return L;
}

const std::vector<std::string>& Parser::errors() const {
    return errs_;
}
