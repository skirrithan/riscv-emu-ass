#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
cd "${SCRIPT_DIR}"/..

# Function to run a test and report status
run_test() {
    local test_name=$1
    local test_cmd=$2
    printf "=== Running %s tests ===\n" "${test_name}"
    if ${test_cmd}; then
        printf "%s tests passed\n" "${test_name}"
        return 0
    else
        printf "%s tests failed\n" "${test_name}"
        return 1
    fi
}

# Clean any previous build
rm -rf build/test_output || true
mkdir -p build/test_output

# Build tests
echo "=== Building tests ==="
cmake -B build -S . || { echo "CMAKE configuration failed"; exit 1; }
cmake --build build -j || { echo "Build failed"; exit 1; }

# Run tests
failed_tests=0

# Run assembler tests
run_test "assembler" "./build/test_assemble" || ((failed_tests++))

# Run disassembler tests
run_test "disassembler" "./build/test_disassemble" || ((failed_tests++))

# Report overall status
echo "=== Test Summary ==="
if [ $failed_tests -eq 0 ]; then
    printf "All tests passed!\n"
    exit 0
else
    printf "%d test suite(s) failed\n" $failed_tests
    exit 1
fi
