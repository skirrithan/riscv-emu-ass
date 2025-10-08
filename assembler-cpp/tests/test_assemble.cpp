#include "assembler/driver.h"
#include <iostream>
#include <filesystem>

int main() {
    namespace fs = std::filesystem;
    std::string base = "tests/data";
    int failed = 0;

    for (auto& f : fs::directory_iterator(base)) {
        if (f.path().extension() == ".s") {
            std::string in = f.path().string();
            auto path_copy = f.path();
            std::string out_bin = path_copy.replace_extension(".bin").string();
            std::string out_hex = path_copy.replace_extension(".hex").string();

            std::cout << "[ASSEMBLE] " << in << "\n";
            int rc = assembleFile(in, out_bin, false);
            if (rc != 0) {
                std::cerr << "Assembly failed for " << in << " (rc=" << rc << ")\n";
                failed++;
                continue;
            }

            assembleFile(in, out_hex, true); // also output readable hex
            std::cout << "Wrote " << out_bin << " and " << out_hex << "\n";
        }
    }
    std::cout << "\nAssembly test done (" << failed << " failed)\n";
    return failed;
}
