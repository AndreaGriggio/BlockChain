CC := gcc
CFLAGS := -std=gnu11 -Wall -Wextra -g -pthread
INCLUDES := -Icode/include -Icode/include/communication -Icode/include/miner -Icode/include/node
LDLIBS := -lcrypto

SRC := code/src
OBJ := code/obj
BIN := code/bin

# I sorgenti ora stanno anche in sottocartelle: diciamo a make dove cercarli
vpath %.c code/src code/src/communication code/src/miner code/src/node

COMMON_OBJS := \
	$(OBJ)/block.o \
	$(OBJ)/protocolSocket.o \
	$(OBJ)/message.o \
	$(OBJ)/childProcess.o \
	$(OBJ)/utils.o

.PHONY: build clean run dirs

build: dirs code/blockchain $(BIN)/miner $(BIN)/MinerLauncher $(BIN)/client $(BIN)/node $(BIN)/ClientsLauncher

dirs:
	mkdir -p $(OBJ) $(BIN)

code/blockchain: $(OBJ)/main.o $(OBJ)/repl.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/miner: $(OBJ)/miner.o $(OBJ)/minerCommunicationProcess.o $(OBJ)/minerStatus.o $(OBJ)/transactionPool.o $(OBJ)/minerCommunicationProtocol.o $(OBJ)/minerFifo.o $(OBJ)/minerThread.o $(OBJ)/blocksPool.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/client: $(OBJ)/ClientProcess.o $(OBJ)/client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/node: $(OBJ)/node.o $(OBJ)/nodeContext.o $(OBJ)/nodeLog.o $(OBJ)/nodeCSV.o $(OBJ)/nodeFIFO.o $(OBJ)/nodeListener.o $(OBJ)/nodeValidation.o $(OBJ)/nodeStatus.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/ClientsLauncher: $(OBJ)/ClientsLauncher.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(BIN)/MinerLauncher: $(OBJ)/MinerLauncher.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(OBJ)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ)/*.o
	rm -f code/blockchain
	rm -f $(BIN)/miner $(BIN)/client $(BIN)/node $(BIN)/ClientsLauncher $(BIN)/MinerLauncher
	rm -f *.log
	rm -f ./tmp/*
	rm -f /dev/shm/sem.blockchain_csv
	rm -f blockchain.csv node_*.csv

run: build
	./code/blockchain $(ARGS)
