CC = g++
DEBUG = -g
CPLUSPLUS = -std=c++11
CFLAGS = -Wall -c $(DEBUG) -I. $(CPLUSPLUS)
LFLAGS = -Wall $(DEBUG)
TARGETS = server client
OBJ = server.o client.o noise.o

all: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) $^

server: server.o noise.o
	$(CC) $(LFLAGS) $^ -o $@

client: client.o noise.o
	$(CC) $(LFLAGS) $^ -o $@

clean:
	rm $(OBJ) $(TARGETS)
