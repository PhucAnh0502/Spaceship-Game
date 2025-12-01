CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lm

CJSON = Lib/cJSON.c
SERVER_SRC = Server/server.c Server/storage.c Server/utils.c Server/handlers/*.c
CLIENT_SRC = Client/client.c

all: server client

server:
	$(CC) $(CFLAGS) $(SERVER_SRC) $(CJSON) -o server_app $(LDFLAGS)

client:
	$(CC) $(CFLAGS) $(CLIENT_SRC) $(CJSON) -o client_app $(LDFLAGS)

clean:
	rm -f server_app client_app