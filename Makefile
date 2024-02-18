CC = gcc
CFLAGS = -Wall 

all: server client

server: server.c utilities.h 
	$(CC) $(CFLAGS) -o server server.c

client: client.c utilities.h 
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client