# Compiler and flags
CC = gcc
CFLAGS = -Wall -lssl -lcrypto

# Targets and files
CLIENT = client
SERVER = server
CLIENT_SRC = client.c
SERVER_SRC = server.c

# Default target
all: build

# Compile target
compile:
	$(CC) -c $(CLIENT_SRC) -o $(CLIENT).o $(CFLAGS)
	$(CC) -c $(SERVER_SRC) -o $(SERVER).o $(CFLAGS)

# Build target
build: compile
	$(CC) $(CLIENT).o -o $(CLIENT) $(CFLAGS)
	$(CC) $(SERVER).o -o $(SERVER) $(CFLAGS)

# Clean target
clean:
	rm -rf $(CLIENT) $(SERVER) *.o
