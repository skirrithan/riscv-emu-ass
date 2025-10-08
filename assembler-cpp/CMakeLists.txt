cmake_minimum_required(VERSION 3.15)
project(assembler_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include paths
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/assembler
    ${CMAKE_SOURCE_DIR}/include/decoder
    ${CMAKE_SOURCE_DIR}/include/common
)

# Source files (recursively collect, excluding main.cpp)
file(GLOB_RECURSE COMMON_SRC_FILES
    "${CMAKE_SOURCE_DIR}/src/assembler/[!m]*.cpp"  # Exclude main.cpp
    "${CMAKE_SOURCE_DIR}/src/decoder/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/common/*.cpp"
)

# Main executable
add_executable(assembler ${COMMON_SRC_FILES} "${CMAKE_SOURCE_DIR}/src/assembler/main.cpp")

# Link your Rust FFI library if needed
target_link_libraries(assembler PRIVATE
    ${CMAKE_SOURCE_DIR}/../isa-ffi/target/release/libisa_ffi.a
)

# Test executables (use common source files only)
add_executable(test_assemble tests/test_assemble.cpp ${COMMON_SRC_FILES})
target_link_libraries(test_assemble PRIVATE
    ${CMAKE_SOURCE_DIR}/../isa-ffi/target/release/libisa_ffi.a
)

add_executable(test_disassemble tests/test_dissasemble.cpp ${COMMON_SRC_FILES})
target_link_libraries(test_disassemble PRIVATE
    ${CMAKE_SOURCE_DIR}/../isa-ffi/target/release/libisa_ffi.a
)
    
