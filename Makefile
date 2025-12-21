CC = gcc
CFLAGS = -Wall -g -ICommon -ILib -IServer -IClient

LDFLAGS_COMMON = -lm

LDFLAGS_CLIENT = -lncurses

CJSON = Lib/cJSON.c

SERVER_HANDLERS = $(wildcard Server/handlers/*.c)
SERVER_SRC = Server/server.c Server/storage.c Server/utils.c $(SERVER_HANDLERS)

CLIENT_SRC = Client/client.c Client/utils.c

.PHONY: all clean

all: server_app client_app

server_app: $(SERVER_SRC) $(CJSON)
	$(CC) $(CFLAGS) $(SERVER_SRC) $(CJSON) -o server_app $(LDFLAGS_COMMON)

client_app: $(CLIENT_SRC) $(CJSON)
	$(CC) $(CFLAGS) $(CLIENT_SRC) $(CJSON) -o client_app $(LDFLAGS_COMMON) $(LDFLAGS_CLIENT)

clean:
	rm -f server_app client_app