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

# Source files (recursively collect)
file(GLOB_RECURSE SRC_FILES
    "${CMAKE_SOURCE_DIR}/src/assembler/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/decoder/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/common/*.cpp"
)

add_executable(assembler ${SRC_FILES})

# Link your Rust FFI library if needed
target_link_libraries(assembler PRIVATE
    ${CMAKE_SOURCE_DIR}/../isa-ffi/target/release/libisa_ffi.a
)
