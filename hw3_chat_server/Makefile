# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -pthread

# Targets
SERVER = hw3server
CLIENT = hw3client

# Source files
SERVER_SRC = hw3server.c
CLIENT_SRC = hw3client.c

# Default target
all: $(SERVER) $(CLIENT)

# Server target
$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER) $(SERVER_SRC)

# Client target
$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT_SRC)

# Clean up build artifacts
clean:
	rm -f $(SERVER) $(CLIENT)

