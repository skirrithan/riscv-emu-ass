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

if(DEFINED RUST_FFI_PATH)
    # Use the path provided via -DRUST_FFI_PATH
    set(RUST_FFI_DIR ${RUST_FFI_PATH})
elseif(DEFINED ENV{RUST_FFI_PATH})
    # Use the path from environment variable
    set(RUST_FFI_DIR $ENV{RUST_FFI_PATH})
else()
    # Default path
    set(RUST_FFI_DIR ${CMAKE_SOURCE_DIR}/../isa-ffi/target/release)
endif()

message(STATUS "Looking for Rust FFI library in: ${RUST_FFI_DIR}")

# Find the Rust FFI library
set(RUST_FFI_LIB "${RUST_FFI_DIR}/libisa_ffi.a")
if(NOT RUST_FFI_LIB)
    message(FATAL_ERROR "Could not find Rust FFI library in ${RUST_FFI_PATH}")
endif()
message(STATUS "Found Rust FFI library: ${RUST_FFI_LIB}")

# Add system libraries that might be needed
find_library(DL_LIBRARY dl)
find_package(Threads)

# Add test_golden executable
add_executable(test_golden
    tests/test_golden.cpp
    ${COMMON_SRC_FILES}
)

# Link your Rust FFI library and system dependencies
target_link_libraries(assembler PRIVATE
    ${RUST_FFI_LIB}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_golden PRIVATE
    ${RUST_FFI_LIB}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Test executables (use common source files only)
add_executable(test_assemble tests/test_assemble.cpp ${COMMON_SRC_FILES})
target_link_libraries(test_assemble PRIVATE
    ${RUST_FFI_LIB}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_executable(test_disassemble tests/test_dissasemble.cpp ${COMMON_SRC_FILES})
target_link_libraries(test_disassemble PRIVATE
    ${RUST_FFI_LIB}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
)
