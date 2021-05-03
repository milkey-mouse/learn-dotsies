CC = gcc
CFLAGS = -std=c99 -Wall -O3
LIBS = -lm -lmsgpackc -lz

.PHONY: default clean
.PRECIOUS: generate filters

default: filters

generate: generate.c murmur3.c murmur3.h xorfilter.h
	$(CC) $(CFLAGS) $(LIBS) generate.c murmur3.c -o $@

filters: generate
	./generate wordfreq.msgpack.gz filters

clean:
	rm -rf generate filters
