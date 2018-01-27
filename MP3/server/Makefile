.PHONY: all clean

# compiler name
CC=g++

# place options like -O2, -g here
CFLAGS=-lssl -lcrypto

all: boss

boss: boss.cpp boss.h
	$(CC) -o boss boss.h boss.cpp $(CFLAGS)

clean:
	rm -f boss
