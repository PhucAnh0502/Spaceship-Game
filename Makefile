CC = gcc
CFLAGS = -Wall -g -I Common -I Lib -I Server -I Client

LDFLAGS_COMMON = -lm -lpthread

LDFLAGS_CLIENT = -lncurses

CJSON = Lib/cJSON.c

SERVER_HANDLERS = $(shell find Server/handlers -name "*.c" ! -name "client_*.c")
SERVER_SRC = Server/server.c Server/services/storage/storage.c Server/services/utils/utils.c $(SERVER_HANDLERS)

CLIENT_SRC = $(shell find Client -name "*.c")

.PHONY: all clean

all: server_app client_app

server_app: $(SERVER_SRC) $(CJSON)
	$(CC) $(CFLAGS) $(SERVER_SRC) $(CJSON) -o server_app $(LDFLAGS_COMMON)

client_app: $(CLIENT_SRC) $(CJSON)
	$(CC) $(CFLAGS) $(CLIENT_SRC) $(CJSON) -o client_app $(LDFLAGS_COMMON) $(LDFLAGS_CLIENT)

clean:
	rm -f server_app client_app
