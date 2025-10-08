#include "assembler/symbols.h"
#include <stdexcept>

void SymbolTable::define(const std::string& name, uint32_t addr){
  map_[name] = addr;
}

bool SymbolTable::isDefined(const std::string& name) const {
  return map_.find(name) != map_.end();
}

uint32_t SymbolTable::get(const std::string& name) const {
  auto it = map_.find(name);
  if (it == map_.end()) throw std::runtime_error("undefined symbol: " + name);
  return it->second;
}
