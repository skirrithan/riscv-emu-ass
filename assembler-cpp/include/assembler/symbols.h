// address mapping for pass 1 & 2
#pragma once
#include <unordered_map>
#include <string>
#include <cstdint>

class SymbolTable {
public:
    void define(const std::string& name, uint32_t addr);
    bool isDefined(const std::string& name) const;
    uint32_t get(const std::string& name) const;
private:
    std::unordered_map<std::string, uint32_t> map_;
};
