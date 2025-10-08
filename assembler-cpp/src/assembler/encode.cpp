#include "assembler/encode.h"
#include "common/utils.h"
#include <cstdint>
#include <stdexcept>
#include <regex>
#include <algorithm>  // sort

// --- tiny helpers (defensively masked) ---
static uint32_t rtype(uint8_t f7, uint8_t rs2, uint8_t rs1, uint8_t f3, uint8_t rd, uint8_t op){
  return (((uint32_t)f7  & 0x7Fu) << 25)
       | (((uint32_t)rs2 & 0x1Fu) << 20)
       | (((uint32_t)rs1 & 0x1Fu) << 15)
       | (((uint32_t)f3  & 0x07u) << 12)
       | (((uint32_t)rd  & 0x1Fu) << 7)
       |  ((uint32_t)op  & 0x7Fu);
}
static uint32_t itype(int32_t imm, uint8_t rs1, uint8_t f3, uint8_t rd, uint8_t op){
  uint32_t u = (uint32_t)imm;
  return ((u & 0xFFFu) << 20)
       | (((uint32_t)rs1 & 0x1Fu) << 15)
       | (((uint32_t)f3  & 0x07u) << 12)
       | (((uint32_t)rd  & 0x1Fu) << 7)
       |  ((uint32_t)op  & 0x7Fu);
}
static uint32_t stype(int32_t imm, uint8_t rs2, uint8_t rs1, uint8_t f3, uint8_t op){
  uint32_t u = (uint32_t)imm;
  uint32_t i11_5 = (u >> 5) & 0x7Fu;
  uint32_t i4_0  =  u       & 0x1Fu;
  return (i11_5 << 25)
       | (((uint32_t)rs2 & 0x1Fu) << 20)
       | (((uint32_t)rs1 & 0x1Fu) << 15)
       | (((uint32_t)f3  & 0x07u) << 12)
       | (i4_0 << 7)
       | ((uint32_t)op & 0x7Fu);
}
static uint32_t btype(int32_t imm, uint8_t rs2, uint8_t rs1, uint8_t f3, uint8_t op){
  uint32_t u    = (uint32_t)imm;
  uint32_t b12  = (u >> 12) & 0x1u;
  uint32_t b10_5= (u >>  5) & 0x3Fu;
  uint32_t b4_1 = (u >>  1) & 0x0Fu;
  uint32_t b11  = (u >> 11) & 0x1u;
  return (b12 << 31)
       | (b10_5 << 25)
       | (((uint32_t)rs2 & 0x1Fu) << 20)
       | (((uint32_t)rs1 & 0x1Fu) << 15)
       | (((uint32_t)f3  & 0x07u) << 12)
       | (b4_1 << 8)
       | (b11 << 7)
       | ((uint32_t)op & 0x7Fu);
}
static uint32_t utype(int32_t imm20, uint8_t rd, uint8_t op){
  return (((uint32_t)imm20 & 0xFFFFFu) << 12)
       | (((uint32_t)rd    & 0x1Fu)    << 7)
       |  ((uint32_t)op    & 0x7Fu);
}
static uint32_t jtype(int32_t imm, uint8_t rd, uint8_t op){
  uint32_t u     = (uint32_t)imm;
  uint32_t j20   = (u >> 20) & 0x1u;
  uint32_t j10_1 = (u >>  1) & 0x3FFu;
  uint32_t j11   = (u >> 11) & 0x1u;
  uint32_t j19_12= (u >> 12) & 0xFFu;
  return (j20 << 31)
       | (j19_12 << 12)
       | (j11 << 20)
       | (j10_1 << 21)
       | (((uint32_t)rd & 0x1Fu) << 7)
       | ((uint32_t)op & 0x7Fu);
}

// --- parsing helpers ---
static bool isNumber(const std::string& s){
  if (s.empty()) return false;
  size_t i = 0;
  if (s[i] == '+' || s[i] == '-') ++i;
  if (i + 2 <= s.size() && s[i] == '0' && (s[i+1]=='x' || s[i+1]=='X')) {
    for (size_t j = i + 2; j < s.size(); ++j)
      if (!std::isxdigit((unsigned char)s[j])) return false;
    return true;
  }
  for (; i < s.size(); ++i)
    if (!std::isdigit((unsigned char)s[i])) return false;
  return true;
}
static int64_t parseInt(const std::string& s){
  // std::stoll handles +/- sign; choose base by prefix (ignoring sign)
  size_t i = (s.size() && (s[0]=='+' || s[0]=='-')) ? 1 : 0;
  int base = (i + 2 <= s.size() && s[i]=='0' && (s[i+1]=='x' || s[i+1]=='X')) ? 16 : 10;
  return std::stoll(s, nullptr, base);
}
static bool parseRegX(const std::string& s, uint8_t& out){
  if (s.size() < 2 || s[0] != 'x') return false;
  for (size_t i = 1; i < s.size(); ++i)
    if (!std::isdigit((unsigned char)s[i])) return false;
  long v = std::stol(s.substr(1));
  if (v < 0 || v > 31) return false;
  out = (uint8_t)v; return true;
}
static bool parseMemOp(const std::string& s, int32_t& off, uint8_t& rs1){
  // forms: imm(rs1)  e.g., 0(x10)  -8(x2)  +0x40(x3)
  std::regex re(R"(^\s*([+-]?(?:0x[0-9a-fA-F]+|\d+))\s*\(\s*x(\d+)\s*\)\s*$)");
  std::smatch m; if(!std::regex_match(s, m, re)) return false;
  off = (int32_t)parseInt(m[1].str());
  long r = std::stol(m[2].str());
  if (r < 0 || r > 31) return false;
  rs1 = (uint8_t)r; return true;
}

// --- encoder orchestration ---
Encoder::Encoder(Program& p, SymbolTable& s):prog_(p),sym_(s){}

std::vector<uint32_t> Encoder::assemble() {
  // --- Pass 1: assign PCs and define labels (labels on label-only lines bind to next instr PC) ---
  pcs_.clear();
  pcs_.reserve(prog_.instrs.size());

  // Sort labels by source line; we’ll consume them as we reach each instruction line.
  std::vector<LabelDef> labels = prog_.labels;
  std::sort(labels.begin(), labels.end(),
            [](const LabelDef& a, const LabelDef& b){ return a.line < b.line; });

  uint32_t pc = 0;
  size_t li = 0; // label index

  for (const auto& ins : prog_.instrs){
    // Define any labels whose line is <= this instruction's line and not yet defined.
    while (li < labels.size() && labels[li].line <= ins.line) {
      if (!sym_.isDefined(labels[li].name))
        sym_.define(labels[li].name, pc);
      ++li;
    }
    pcs_.push_back(pc);
    pc += 4; // RV32I fixed 4-byte instructions
  }

  // Any remaining labels (at EOF or after the last instruction) bind to final pc.
  while (li < labels.size()) {
    if (!sym_.isDefined(labels[li].name))
      sym_.define(labels[li].name, pc);
    ++li;
  }

  // --- Pass 2: encode ---
  std::vector<uint32_t> out;
  out.reserve(prog_.instrs.size());
  for (size_t i = 0; i < prog_.instrs.size(); ++i){
    uint32_t w = encodeInstr(prog_.instrs[i], pcs_[i]);
    out.push_back(w);
  }
  return out;
}

uint32_t Encoder::encodeInstr(const AsmInstr& ins, uint32_t pc){
  auto M = ins.mnemonic;
  // Upper-case normalize (ASCII-safe)
  for (auto& c : M) c = (char)std::toupper((unsigned char)c);

  auto arg = [&](int k)->std::string {
    if ((size_t)k >= ins.args.size()) throw std::runtime_error("operand count mismatch for "+M);
    return ins.args[(size_t)k];
  };

  // helpers to get reg / imm / sym-or-imm
  auto wantReg = [&](const std::string& s)->uint8_t{
    uint8_t r; if(!parseRegX(s,r)) throw std::runtime_error("expected register, got '"+s+"'");
    return r;
  };
  auto wantImm = [&](const std::string& s)->int32_t{
    if(!isNumber(s)) throw std::runtime_error("expected immediate, got '"+s+"'");
    long long v = parseInt(s);
    return (int32_t)v;
  };
  auto resolveSymOrImm = [&](const std::string& s, uint32_t atPc, bool pcRel=false)->int32_t{
    if (isNumber(s)) return (int32_t)parseInt(s);
    if (!sym_.isDefined(s)) throw std::runtime_error("undefined symbol: "+s);
    int64_t target = sym_.get(s);
    if (!pcRel) return (int32_t)target;
    int64_t delta = (int64_t)target - (int64_t)atPc;
    return (int32_t)delta;
  };

  // --- Encoding per mnemonic ---
  if (M=="ADD"){
    if (ins.args.size()!=3) throw std::runtime_error("ADD rd, rs1, rs2");
    uint8_t rd=wantReg(arg(0)), rs1=wantReg(arg(1)), rs2=wantReg(arg(2));
    return rtype(0x00, rs2, rs1, 0x0, rd, 0x33);
  }
  if (M=="SUB"){
    if (ins.args.size()!=3) throw std::runtime_error("SUB rd, rs1, rs2");
    uint8_t rd=wantReg(arg(0)), rs1=wantReg(arg(1)), rs2=wantReg(arg(2));
    return rtype(0x20, rs2, rs1, 0x0, rd, 0x33);
  }
  if (M=="ADDI"){
    if (ins.args.size()!=3) throw std::runtime_error("ADDI rd, rs1, imm");
    uint8_t rd=wantReg(arg(0)), rs1=wantReg(arg(1)); int32_t imm=wantImm(arg(2));
    if (imm < -2048 || imm > 2047) throw std::runtime_error("ADDI imm out of range");
    return itype(imm, rs1, 0x0, rd, 0x13);
  }
  if (M=="LW"){
    if (ins.args.size()!=2) throw std::runtime_error("LW rd, off(rs1)");
    uint8_t rd = wantReg(arg(0));
    int32_t off; uint8_t rs1;
    if (!parseMemOp(arg(1), off, rs1)) throw std::runtime_error("LW expects off(rs1)");
    if (off < -2048 || off > 2047) throw std::runtime_error("LW offset out of range");
    return itype(off, rs1, 0x2, rd, 0x03);
  }
  if (M=="SW"){
    if (ins.args.size()!=2) throw std::runtime_error("SW rs2, off(rs1)");
    uint8_t rs2 = wantReg(arg(0));
    int32_t off; uint8_t rs1;
    if (!parseMemOp(arg(1), off, rs1)) throw std::runtime_error("SW expects off(rs1)");
    if (off < -2048 || off > 2047) throw std::runtime_error("SW offset out of range");
    return stype(off, rs2, rs1, 0x2, 0x23);
  }
  if (M=="BEQ"){
    if (ins.args.size()!=3) throw std::runtime_error("BEQ rs1, rs2, label");
    uint8_t rs1=wantReg(arg(0)), rs2=wantReg(arg(1));
    int32_t imm = resolveSymOrImm(arg(2), pc, /*pcRel*/true);
    // branch immediate is relative to pc; must be even; byte range in [-4096, +4094]
    if ((imm & 0x1) != 0) throw std::runtime_error("BEQ target misaligned");
    if (imm < -(1<<12) || imm > ((1<<12)-2)) throw std::runtime_error("BEQ out of range");
    return btype(imm, rs2, rs1, 0x0, 0x63);
  }
  if (M == "LUI") {
    if (ins.args.size() != 2) throw std::runtime_error("LUI rd, imm20");
    uint8_t rd = wantReg(arg(0));
    int64_t v = parseInt(arg(1));
    // Use lower 20 bits as immediate field directly.
    int32_t imm20 = (int32_t)(v & 0xFFFFF);
    return utype(imm20, rd, 0x37);
  }
  if (M=="AUIPC"){
    if (ins.args.size()!=2) throw std::runtime_error("AUIPC rd, imm20");
    uint8_t rd=wantReg(arg(0)); int64_t v=parseInt(arg(1));
    int32_t imm20 = (int32_t)((uint32_t)v >> 12);
    return utype(imm20, rd, 0x17);
  }
  if (M=="JAL"){
    if (ins.args.size()!=2) throw std::runtime_error("JAL rd, label");
    uint8_t rd=wantReg(arg(0));
    int32_t imm = resolveSymOrImm(arg(1), pc, /*pcRel*/true);
    // JAL immediate is relative to pc; must be even; byte range in [-(1<<20), (1<<20)-2]
    if ((imm & 0x1) != 0) throw std::runtime_error("JAL target misaligned");
    if (imm < -(1<<20) || imm > ((1<<20)-2)) throw std::runtime_error("JAL out of range");
    return jtype(imm, rd, 0x6F);
  }
  if (M=="JALR"){
    if (ins.args.size()!=3) throw std::runtime_error("JALR rd, rs1, imm");
    uint8_t rd=wantReg(arg(0)), rs1=wantReg(arg(1)); int32_t imm=wantImm(arg(2));
    if (imm < -2048 || imm > 2047) throw std::runtime_error("JALR imm out of range");
    return itype(imm, rs1, 0x0, rd, 0x67);
  }

  throw std::runtime_error("unknown mnemonic: "+M);
}
