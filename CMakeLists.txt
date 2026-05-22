cmake_minimum_required(VERSION 3.28)
project(Gonzato_Luongo_Griggio C)

set(CMAKE_C_STANDARD 11)

#Ordinare File in ordine alfabetico nella lista degli eseguibili
set(SOURCES
        code/src/block.c
        code/src/childProcess.c
        code/src/main.c
        code/include/utils.h
)

add_executable(Gonzato_Luongo_Griggio ${SOURCES})

target_include_directories(Gonzato_Luongo_Griggio PRIVATE code/include)

