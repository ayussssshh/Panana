cmake_minimum_required(VERSION 3.16)
project(panana C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets clangd understand your project

add_executable(panana
    src/main.c
    src/websocket.c
)

target_compile_options(panana PRIVATE -Wall -Wextra -g)

# later, for TLS and SHA-1:
# find_package(OpenSSL REQUIRED)
# target_link_libraries(panana PRIVATE OpenSSL::SSL OpenSSL::Crypto)