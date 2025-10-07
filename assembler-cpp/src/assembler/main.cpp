// CLI: assembler in.s -o out.bin --hex
#include "assembler/driver.h"
#include <iostream>
#include <string>

int main(int argc, char** argv){
  if (argc < 4){
    std::cerr << "usage: assembler in.s -o out.bin [--hex]\n";
    return 64;
  }
  std::string inFile = argv[1], outFile; bool hex=false;
  for (int i=2;i<argc;i++){
    std::string a = argv[i];
    if (a=="-o" && i+1<argc) outFile = argv[++i];
    else if (a=="--hex") hex = true;
  }
  if (outFile.empty()){ std::cerr << "missing -o <outfile>\n"; return 64; }
  return assembleFile(inFile, outFile, hex);
}
