cmake_minimum_required(VERSION 3.28)
project(Gonzato_Luongo_Griggio C)

set(CMAKE_C_STANDARD 11)

add_executable(Gonzato_Luongo_Griggio ${SOURCES}
        code/src/main.c
        code/include/blocco.h
        code/include/miner.h
        code/include/childProcess.h
        code/src/childProcess.c)
target_include_directories(Gonzato_Luongo_Griggio PRIVATE code/include)

