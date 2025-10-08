#include "decoder/disassembler_driver.h"
#include <iostream>
#include <filesystem>

int main() {
    namespace fs = std::filesystem;
    std::string base = "tests/data";
    int failed = 0;

    for (auto& f : fs::directory_iterator(base)) {
        if (f.path().extension() == ".bin") {
            std::cout << "[DISASM] " << f.path().filename().string() << "\n";
            int rc = disassembleFile(f.path().string(), true, true);
            if (rc != 0) {
                std::cerr << "Disasm failed for " << f.path() << "\n";
                failed++;
            }
        }
    }
    std::cout << "\nDisassembly test done (" << failed << " failed)\n";
    return failed;
}
