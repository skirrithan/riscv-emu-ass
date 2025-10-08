#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"/..

echo "=== Building assembler and disassembler ==="
cmake -B build -S .
cmake --build build -j

echo "=== Running assembler tests ==="
./build/test_assemble

echo "=== Running disassembler tests ==="
./build/test_disassemble
