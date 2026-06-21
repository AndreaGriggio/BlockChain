cmake_minimum_required(VERSION 3.28)
project(Gonzato_Luongo_Griggio C)

set(CMAKE_C_STANDARD 11)

find_package(OpenSSL REQUIRED)

add_executable(Gonzato_Luongo_Griggio
        code/src/main.c
        code/src/childProcess.c
        code/src/node.c
        code/src/miner.c
        code/src/client.c
        code/src/block.c
        code/include/error.h
        code/include/message.h
        code/src/message.c
        code/src/utils.c
        code/src/ClientProcess.c
        code/src/protocolSocket.c
        code/include/constants.h
        code/src/minerCommunicationProcess.c
        code/include/minerStatus.h
        code/src/minerStatus.c
        code/src/minerCommunicationProtocol.c
        code/include/minerCommunicationProtocol.h
        code/include/transactionPool.h
        code/src/transactionPool.c
        code/src/blocksPool.c
        code/include/blocksPool.h
        code/src/minerFifo.c
        code/include/minerFifo.h
        code/src/minerThread.c
        code/include/minerThread.h
)

target_include_directories(Gonzato_Luongo_Griggio PRIVATE code/include)

target_link_libraries(Gonzato_Luongo_Griggio PRIVATE OpenSSL::Crypto OpenSSL::SSL)


