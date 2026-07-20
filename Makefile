CC := gcc
CFLAGS := -std=gnu11 -Wall -Wextra -g -pthread

ARGS ?= 2 2 1 1 12

INCLUDES := -Icode/include -Icode/include/communication -Icode/include/miner -Icode/include/node
LDLIBS := -lcrypto
RUNTIME_DIR := ./code/tmp

SRC := code/src
OBJ := code/obj
BIN := code/bin

# Sottocartelle dei sorgenti per vpath
SRC_DIRS := $(SRC) $(SRC)/communication $(SRC)/miner $(SRC)/node

vpath %.c $(SRC_DIRS)

COMMON_OBJS := \
    $(OBJ)/block.o \
    $(OBJ)/protocolSocket.o \
    $(OBJ)/message.o \
    $(OBJ)/childProcess.o \
    $(OBJ)/utils.o

.PHONY: build clean run dirs

# Il target build ora dipende solo dai file finali
build: code/blockchain $(BIN)/broker $(BIN)/miner $(BIN)/client $(BIN)/node

# Regole per creare le cartelle al volo se non esistono
$(OBJ) $(BIN):
	mkdir -p $@

code/blockchain: $(OBJ)/main.o $(OBJ)/repl.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/broker: $(OBJ)/broker.o $(COMMON_OBJS) | $(BIN)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/miner: $(OBJ)/miner.o $(OBJ)/minerCommunicationProcess.o $(OBJ)/minerStatus.o $(OBJ)/transactionPool.o $(OBJ)/minerCommunicationProtocol.o $(OBJ)/minerFifo.o $(OBJ)/minerThread.o $(OBJ)/blocksPool.o $(COMMON_OBJS) | $(BIN)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/client: $(OBJ)/ClientProcess.o $(OBJ)/client.o $(COMMON_OBJS) | $(BIN)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/node: $(OBJ)/node.o $(OBJ)/nodeContext.o $(OBJ)/nodeLog.o $(OBJ)/nodeCSV.o $(OBJ)/nodeFIFO.o $(OBJ)/nodeListener.o $(OBJ)/nodeValidation.o $(OBJ)/nodeStatus.o $(COMMON_OBJS) | $(BIN)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

# Il simbolo '| $(OBJ)' garantisce che la cartella obj esista PRIMA di compilare i file .o
$(OBJ)/%.o: %.c | $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ) $(BIN)
	rm -f code/blockchain
	rm -f *.log
	rm -f blockchain.csv node_*_blockchain.csv
	rm -rf $(RUNTIME_DIR)
	rm -f /dev/shm/sem.blockchain_csv
	rm -f /dev/shm/sem.blockchain_broker

run: build
	./code/blockchain $(ARGS)