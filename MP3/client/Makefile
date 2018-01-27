.PHONY: all clean

# compiler name
CC=gcc

# place options like -O2, -g here
CFLAGS=-lssl -lcrypto

all: miner

miner: miner.c
	$(CC) -o miner miner.c -lssl -lcrypto

clean:
	rm -f miner
