CC := gcc
CFLAGS := -std=gnu11 -Wall -Wextra -g -pthread
INCLUDES := -Icode/include
LDLIBS := -lcrypto

SRC := code/src
OBJ := code/obj
BIN := code/bin

COMMON_OBJS := \
	$(OBJ)/block.o \
	$(OBJ)/protocolSocket.o \
	$(OBJ)/message.o \
	$(OBJ)/childProcess.o \
	$(OBJ)/utils.o

.PHONY: build clean run dirs

build: dirs code/blockchain $(BIN)/miner $(BIN)/MinerLauncher $(BIN)/client $(BIN)/node $(BIN)/ClientsLauncher

# $(BIN)/miner escluso per il momento sia da build che da 

dirs:
	mkdir -p $(OBJ) $(BIN)

code/blockchain: $(OBJ)/main.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/miner: $(OBJ)/miner.o $(OBJ)/minerCommunicationProcess.o $(OBJ)/minerStatus.o $(OBJ)/transactionPool.o $(OBJ)/minerCommunicationProtocol.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/client: $(OBJ)/ClientProcess.o $(OBJ)/client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/node: $(OBJ)/node.o $(OBJ)/nodeStatus.o $(COMMON_OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDLIBS)

$(BIN)/ClientsLauncher: $(OBJ)/ClientsLauncher.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ 

$(BIN)/MinerLauncher: $(OBJ)/MinerLauncher.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ)/*.o
	rm -f code/blockchain
	rm -f $(BIN)/miner $(BIN)/client $(BIN)/node $(BIN)/ClientsLauncher $(BIN)/MinerLauncher
	rm -f *.log
	rm -f ./tmp/*.sock

run: build
	./code/blockchain $(ARGS)