CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g

TARGET = blockchain
SRC = src/main.c

build:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: build
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o
