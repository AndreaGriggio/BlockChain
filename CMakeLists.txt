cmake_minimum_required(VERSION 3.28)
project(Progetto C)

set(CMAKE_C_STANDARD 11)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.c)

add_executable(Progetto ${SOURCES}
        code/src/main.c)


