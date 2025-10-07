#include "assembler/symbols.h"
#include <stdexcept>

void SymbolTable::define(const std::string& name, uint32_t addr){
  auto it = map_.find(name);
  if (it != map_.end()) { it->second.addr = addr; it->second.defined = true; return; }
  map_[name] = Symbol{name, addr, true};
}
bool SymbolTable::isDefined(const std::string& name) const {
  auto it = map_.find(name); return it!=map_.end() && it->second.defined;
}
uint32_t SymbolTable::get(const std::string& name) const {
  auto it = map_.find(name);
  if (it==map_.end() || !it->second.defined) throw std::runtime_error("undefined symbol: "+name);
  return it->second.addr;
}
