CC = g++
DEBUG = -g
CFLAGS = -Wall -c $(DEBUG) -I.
LFLAGS = -Wall $(DEBUG)
TARGETS = server client
SRC = server.c client.c
HDR = 
OBJ = server.o client.o

all: $(TARGETS)

server.o: server.c
	$(CC) $(CFLAGS) $<

client.o: client.c
	$(CC) $(CFLAGS) $<

server: server.o
	$(CC) $(LFLAGS) $^ -o $@

client: client.o
	$(CC) $(LFLAGS) $^ -o $@

clean:
	rm *o $(TARGETS)
