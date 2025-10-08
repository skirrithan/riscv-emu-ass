#include "assembler/driver.h"
#include "assembler/lexer.h"
#include "assembler/parser.h"
#include "assembler/encode.h"
#include "assembler/symbols.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <iomanip>

int assembleFile(const std::string& inPath, const std::string& outPath, bool hex) {
  auto src = readFileToString(inPath);
  if (src.empty()) { std::cerr << "Empty or unreadable input.\n"; return 1; }

  Lexer lx(src); lx.tokenize();
  Parser ps(lx.tokenize());
  Program prog = ps.parse();
  if (!ps.errors().empty()){
    for (auto& e: ps.errors()) std::cerr << e << "\n";
    return 2;
  }

  SymbolTable syms;
  Encoder enc(prog, syms);
  std::vector<uint32_t> words;
  try {
    words = enc.assemble();
  } catch (const std::exception& ex){
    std::cerr << "assemble error: " << ex.what() << "\n";
    return 3;
  }

  if (!hex) {
    if (!writeBinaryWords(outPath, words)) return 4;
  } else {
    std::ofstream f(outPath);
    if (!f) { std::cerr << "open fail: " << outPath << "\n"; return 4; }
    for (uint32_t w : words) {
      f << std::hex << std::setw(8) << std::setfill('0') << w << "\n";
    }
  }
  return 0;
}
