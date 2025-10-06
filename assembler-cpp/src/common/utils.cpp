#include "common/utils.h"
#include <fstream>
#include <iterator>
#include <iostream>

std::string readFileToString(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) {
        std::cerr << "Error: failed to open file: " << path << "\n";
        return {};
    }
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return content;
}

std::vector<uint8_t> readBinaryFile(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) {
        std::cerr << "Error: failed to open binary file: " << path << "\n";
        return {};
    }
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    f.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

bool writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::out | std::ios::binary);
    if (!f) {
        std::cerr << "Error: failed to open file for writing: " << path << "\n";
        return false;
    }
    f.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    return true;
}

bool writeBinaryWords(const std::string& path, const std::vector<uint32_t>& words) {
    std::ofstream f(path, std::ios::out | std::ios::binary);
    if (!f) {
        std::cerr << "Error: failed to open file for writing: " << path << "\n";
        return false;
    }
    for (uint32_t w : words) {
        // Little-endian write
        uint8_t bytes[4] = {
            (uint8_t)(w & 0xFF),
            (uint8_t)((w >> 8) & 0xFF),
            (uint8_t)((w >> 16) & 0xFF),
            (uint8_t)((w >> 24) & 0xFF)
        };
        f.write(reinterpret_cast<const char*>(bytes), 4);
    }
    return true;
}
