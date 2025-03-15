BUILD ?= debug
CC = gcc

ifeq ($(BUILD), debug)
	CCFLAGS = -c -g -O0 -std=c99 -pedantic
else ifeq ($(BUILD), release)
	CCFLAGS = -c -g -O2 -std=c99 -pedantic
else ifeq ($(BUILD), dist)
	CCFLAGS = -c -O3 -std=c99 -pedantic
endif

.PHONY: mkdirs clean

all: clean mkdirs bin/huffman

mkdirs:
	mkdir -p bin/o/

clean:
	rm -rf bin/

bin/huffman: bin/o/huffman.o bin/o/bit_stream.o
	$(CC) -o $@ $^

bin/o/%.o: comp/%.c
	$(CC) $(CCFLAGS) -o $@ $<
