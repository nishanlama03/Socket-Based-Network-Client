CC=cc
CFLAGS=-g 

all: server client service

server : server.c
	$(CC) $(CFLAGS) -o $@ $?

client : client.c
	$(CC) $(CFLAGS) -o $@ $?

service : service.c
	$(CC) $(CFLAGS) -o $@ $?

clean:
	rm -rf *.o client server service *~
