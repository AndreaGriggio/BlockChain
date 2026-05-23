cmake_minimum_required(VERSION 3.28)
project(Gonzato_Luongo_Griggio C)

set(CMAKE_C_STANDARD 11)

find_package(OpenSSL REQUIRED)

add_executable(Gonzato_Luongo_Griggio
        src/main.c
        src/childProcess.c
        src/node.c
        src/miner.c
        src/client.c
        src/crypto.c
        src/block.c
        code/src/crypto.c)

target_include_directories(Gonzato_Luongo_Griggio PRIVATE include)

target_link_libraries(Gonzato_Luongo_Griggio PRIVATE OpenSSL::Crypto OpenSSL::SSL)


