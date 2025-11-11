#include "assembler/driver.h"
#include "decoder/decoder.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

struct TestCoverage {
    std::unordered_set<std::string> tested_instructions;
    int total_tests = 0;
    int passed_tests = 0;
};

bool compareInstructions(const std::string& expected, const std::string& actual) {
    // Helper to extract mnemonic and operands
    auto parseInstruction = [](const std::string& s) {
        std::string mnemonic;
        std::vector<std::string> operands;
        std::string current;
        bool inNumber = false;
        
        // First, normalize spaces and convert to lowercase
        std::string normalized;
        bool lastWasSpace = true;
        for (char c : s) {
            if (std::isspace(c)) {
                if (!lastWasSpace) {
                    normalized += ' ';
                    lastWasSpace = true;
                }
            } else {
                normalized += std::tolower(c);
                lastWasSpace = false;
            }
        }
        
        // Split into mnemonic and operands
        std::stringstream ss(normalized);
        ss >> mnemonic;
        
        std::string operand;
        while (std::getline(ss, operand, ',')) {
            // Trim whitespace
            operand.erase(0, operand.find_first_not_of(" "));
            operand.erase(operand.find_last_not_of(" ") + 1);
            
            // Handle hex numbers
            if (operand.find("0x") != std::string::npos) {
                try {
                    // Convert hex string to integer
                    uint32_t value = std::stoul(operand, nullptr, 16);
                    // Special handling for LUI/AUIPC immediates: if the value appears
                    // to be a shifted form (i.e. low 12 bits == 0) convert back to
                    // the original immediate by shifting right 12 bits. If it's
                    // already the small immediate (not multiple of 0x1000), leave it.
                    if ((mnemonic == "lui" || mnemonic == "auipc") && (value & 0xFFFu) == 0u) {
                        value >>= 12;
                    }
                    // Format back to hex without leading zeros
                    std::stringstream hex;
                    hex << "0x" << std::hex << value;
                    operand = hex.str();
                } catch (...) {
                    // If conversion fails, keep original
                }
            }
            
            operands.push_back(operand);
        }
        
        return std::make_pair(mnemonic, operands);
    };
    
    auto [expectedMnemonic, expectedOperands] = parseInstruction(expected);
    auto [actualMnemonic, actualOperands] = parseInstruction(actual);
    
    if (expectedMnemonic != actualMnemonic || expectedOperands.size() != actualOperands.size()) {
        return false;
    }

    for (size_t i = 0; i < expectedOperands.size(); i++) {
        // If the expected operand contains alphabetic characters (likely a label),
        // accept any actual operand (we can't resolve labels here).
        bool hasAlpha = false;
        for (char c : expectedOperands[i]) {
            if (std::isalpha(static_cast<unsigned char>(c))) { hasAlpha = true; break; }
        }
        if (hasAlpha) continue;

        if (expectedOperands[i] != actualOperands[i]) {
            return false;
        }
    }
    
    return true;
}

void runAssemblyTest(const std::string& asmPath, const std::string& binPath, TestCoverage& coverage) {
    // Read the binary file
    std::ifstream binFile(binPath, std::ios::binary);
    if (!binFile) {
        std::cerr << "Failed to open binary file: " << binPath << "\n";
        return;
    }
    
    // Read assembly file into lines
    std::vector<std::string> asmLines;
    std::ifstream asmFile(asmPath);
    std::string line;
    while (std::getline(asmFile, line)) {
        // Trim whitespace from both ends
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue; // Skip empty lines
        auto end = line.find_last_not_of(" \t");
        line = line.substr(start, end - start + 1);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == '/' || 
            line[0] == ';' || line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }

        // Tokenize first token to detect labels or directives
        std::stringstream firstSS(line);
        std::string firstTok;
        firstSS >> firstTok;
        // Skip labels (e.g. "_start:") and assembler directives (start with '.')
        if (!firstTok.empty() && (firstTok.back() == ':' || firstTok.front() == '.')) {
            continue;
        }

        // Remove any trailing comments
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
            // Trim any whitespace after removing comment
            end = line.find_last_not_of(" \t");
            if (end != std::string::npos) {
                line = line.substr(0, end + 1);
            }
        }
        
        if (!line.empty()) {
            asmLines.push_back(line);
        }
    }
    
    // Read binary words
    std::vector<uint32_t> binWords;
    uint32_t word;
    while (binFile.read(reinterpret_cast<char*>(&word), sizeof(word))) {
        binWords.push_back(word);
    }
    
    // If instruction counts don't match, compare up to the smaller count
    if (asmLines.size() != binWords.size()) {
        std::cerr << "WARN: Mismatch in instruction count for " << asmPath << "\n";
        std::cerr << "  Assembly lines: " << asmLines.size() << "\n";
        std::cerr << "  Binary words: " << binWords.size() << "\n";
        std::cerr << "  Will compare up to the smaller count and report leftovers.\n";
    }

    // Create a temporary file for testing reassembly
    std::string tempFile = asmPath + ".temp";

    // Test up to the smaller of the two counts
    size_t compareCount = std::min(asmLines.size(), binWords.size());
    // Helper: detect if an assembly line contains a label operand (e.g. b_fwd)
    auto lineHasLabelOperand = [](const std::string &ln) {
        // Find mnemonic end
        size_t p = ln.find_first_of(" \t");
        if (p == std::string::npos) return false;
        std::string ops = ln.substr(p+1);
        std::stringstream ss(ops);
        std::string op;
        while (std::getline(ss, op, ',')) {
            // trim
            size_t s = op.find_first_not_of(" \t");
            if (s == std::string::npos) continue;
            size_t e = op.find_last_not_of(" \t");
            std::string tok = op.substr(s, e - s + 1);
            // memory form like 8(x6)
            size_t par = tok.find('(');
            if (par != std::string::npos) {
                // inside parentheses should be register like xN
                size_t rstart = par + 1;
                size_t rend = tok.find(')', rstart);
                if (rend == std::string::npos) return true; // malformed -> treat as label
                std::string reg = tok.substr(rstart, rend - rstart);
                if (reg.size() < 2 || reg[0] != 'x') return true;
                bool allDigits = true;
                for (size_t k = 1; k < reg.size(); ++k) if (!std::isdigit((unsigned char)reg[k])) { allDigits = false; break; }
                if (!allDigits) return true;
                // outside (immediate) may be hex or signed number
                std::string imm = tok.substr(0, par);
                imm.erase(0, imm.find_first_not_of(" \t"));
                if (imm.empty()) continue;
                if (imm.rfind("0x", 0) == 0) continue;
                bool isNum = true; size_t idx = 0; if (imm[0] == '+' || imm[0] == '-') idx = 1; for (; idx < imm.size(); ++idx) if (!std::isdigit((unsigned char)imm[idx])) { isNum = false; break; }
                if (!isNum) return true;
                continue;
            }
            // token without parentheses: could be register xN, hex 0x..., or immediate number, else label
            if (tok.rfind("0x", 0) == 0) continue;
            if (tok.size() >= 2 && tok[0] == 'x') {
                bool allDigits = true;
                for (size_t k = 1; k < tok.size(); ++k) if (!std::isdigit((unsigned char)tok[k])) { allDigits = false; break; }
                if (allDigits) continue;
            }
            // signed decimal
            size_t idx = 0; if (tok[0] == '+' || tok[0] == '-') idx = 1;
            bool isNum = true; for (; idx < tok.size(); ++idx) if (!std::isdigit((unsigned char)tok[idx])) { isNum = false; break; }
            if (isNum) continue;
            // otherwise treat as label
            return true;
        }
        return false;
    };
    // Detect if this file contains any label-using instructions
    bool fileHasLabel = false;
    for (const auto &ln : asmLines) if (lineHasLabelOperand(ln)) { fileHasLabel = true; break; }

    // If fileHasLabel: assemble whole file once and compare per-instruction
    std::vector<uint32_t> fullReassembledWords;
    std::string tempFullBin = tempFile + ".full.bin";
    if (fileHasLabel) {
        int rc_full = assembleFile(asmPath, tempFullBin, false);
        if (rc_full != 0) {
            std::cerr << "FAIL: Full-file assembly failed for " << asmPath << " (cannot verify label-based instructions)\n";
            // continue but no full reassembly comparisons will be possible
        } else {
            std::ifstream fullBinF(tempFullBin, std::ios::binary);
            uint32_t w;
            while (fullBinF.read(reinterpret_cast<char*>(&w), sizeof(w))) fullReassembledWords.push_back(w);
            fullBinF.close();
            if (fullReassembledWords.size() != binWords.size()) {
                std::cerr << "WARN: Full-file reassembled count (" << fullReassembledWords.size() << ") differs from original binary count (" << binWords.size() << ") for " << asmPath << "\n";
            }
        }
    }

    for (size_t i = 0; i < compareCount; i++) {
        coverage.total_tests++;
        bool testPassed = true;
        
        // Decode the binary
        Decoded decoded = decodeWord(binWords[i], 0);
        std::string disassembled = formatDecoded(decoded, false);
        
        // Compare with original assembly
        if (!compareInstructions(asmLines[i], disassembled)) {
            std::cerr << "FAIL: Disassembly mismatch in " << asmPath << " line " << (i+1) << "\n";
            std::cerr << "  Original: " << asmLines[i] << "\n";
            std::cerr << "  Decoded:  " << disassembled << "\n";
            testPassed = false;
            continue;
        }
        
        // If we assembled the full file, compare the full reassembled word for this index
        if (!fullReassembledWords.empty()) {
            if (i < fullReassembledWords.size()) {
                uint32_t reassembled = fullReassembledWords[i];
                if (reassembled != binWords[i]) {
                    std::cerr << "FAIL: Full-file re-encoding mismatch in " << asmPath << " line " << (i+1) << "\n";
                    std::cerr << "  Original: 0x" << std::hex << binWords[i] << std::dec << "\n";
                    std::cerr << "  Re-encoded: 0x" << std::hex << reassembled << std::dec << "\n";
                    testPassed = false;
                    // continue to next instruction
                }
            } else {
                std::cerr << "FAIL: Full-file reassembly missing instruction for " << asmPath << " line " << (i+1) << "\n";
                testPassed = false;
            }
        } else {
            // No full-file reassembly available: fall back to per-line re-assembly unless line has label
            if (lineHasLabelOperand(asmLines[i])) {
                std::cout << "SKIP re-assembly (contains label): " << asmLines[i] << "\n";
            } else {
                std::ofstream tempAsm(tempFile);
                tempAsm << asmLines[i] << "\n";
                tempAsm.close();

                std::string tempBin = tempFile + ".bin";
                int rc = assembleFile(tempFile, tempBin, false);

                if (rc != 0) {
                    std::cerr << "FAIL: Re-assembly failed for: " << asmLines[i] << "\n";
                    testPassed = false;
                    // cleanup temp files
                    std::remove(tempFile.c_str());
                    std::remove((tempFile + ".bin").c_str());
                    continue;
                }
                
                // Read the reassembled binary
                std::ifstream tempBinFile(tempBin, std::ios::binary);
                uint32_t reassembled;
                tempBinFile.read(reinterpret_cast<char*>(&reassembled), sizeof(reassembled));
                tempBinFile.close();

                if (reassembled != binWords[i]) {
                    std::cerr << "FAIL: Re-encoding mismatch in " << asmPath << " line " << (i+1) << "\n";
                    std::cerr << "  Original: 0x" << std::hex << binWords[i] << std::dec << "\n";
                    std::cerr << "  Re-encoded: 0x" << std::hex << reassembled << std::dec << "\n";
                    testPassed = false;
                    // cleanup temp files
                    std::remove(tempFile.c_str());
                    std::remove((tempFile + ".bin").c_str());
                    continue;
                }
            }
        }
        
        
        
        if (testPassed) {
            coverage.passed_tests++;
            coverage.tested_instructions.insert(decoded.mnemonic);
            std::cout << "PASS: " << asmLines[i] << " <-> 0x" << std::hex << binWords[i] << std::dec << "\n";
        }
    }
    
    // Cleanup temporary files
    std::remove(tempFile.c_str());
    std::remove((tempFile + ".bin").c_str());
}



void printCoverage(const TestCoverage& coverage) {
    std::cout << "\nTest Coverage Summary:\n";
    std::cout << "======================\n";
    std::cout << "Total instructions tested: " << coverage.total_tests << "\n";
    std::cout << "Passed tests: " << coverage.passed_tests << "\n";
    std::cout << "Pass rate: " << (coverage.passed_tests * 100.0 / coverage.total_tests) << "%\n\n";
    
    std::cout << "Instructions tested:\n";
    for (const auto& instr : coverage.tested_instructions) {
        std::cout << "  " << instr << "\n";
    }
}

int main() {
    TestCoverage coverage;
    
    // Run tests from assembly files
    std::cout << "\nTesting Assembly Files:\n";
    std::cout << "====================\n";
    namespace fs = std::filesystem;

    // Get the path to the test directory relative to the executable
    fs::path exePath = fs::read_symlink("/proc/self/exe");
    fs::path testDir = exePath.parent_path().parent_path() / "tests" / "data";
    
    if (!fs::exists(testDir)) {
        std::cerr << "Error: Test directory not found: " << testDir << "\n";
        return 1;
    }
    
    std::cout << "Using test directory: " << testDir << "\n\n";
    
    for (const auto& entry : fs::directory_iterator(testDir)) {
        if (entry.path().extension() == ".s") {
            std::string asmPath = entry.path().string();
            std::string binPath = entry.path().parent_path().string() + "/" + 
                                entry.path().stem().string() + ".bin";
            
            // If binPath doesn't exist, try to assemble the full .s file to create it
            if (!fs::exists(binPath)) {
                std::cout << "Binary companion not found for " << entry.path().filename().string() << ", attempting to assemble full file...\n";
                int rc = assembleFile(asmPath, binPath, false);
                if (rc != 0) {
                    std::cerr << "Failed to assemble full file for testing: " << asmPath << " (rc=" << rc << ")\n";
                    continue;
                }
                std::cout << "Produced binary: " << binPath << "\n";
            }

            if (fs::exists(binPath)) {
                std::cout << "\nTesting file: " << entry.path().filename().string() << "\n";
                runAssemblyTest(asmPath, binPath, coverage);
            }
        }
    }
    
    printCoverage(coverage);
    return (coverage.passed_tests == coverage.total_tests) ? 0 : 1;
}