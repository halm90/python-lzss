CC=gcc
CFLAGS=-Os -Wall -Wextra -Wconversion -ansi -pedantic

OBJECTS = lzss.o encode.o decode.o

UNAME := $(shell $(CC) -dumpmachine 2>&1 | grep -E -o "linux|darwin")

ifeq ($(UNAME), linux)
	LDFLAGS=-Wl,--gc-sections
endif

all: lzss

lzss.o: lzss.c
	$(CC) $(CFLAGS) -c lzss.c

encode.o: encode.c
	$(CC) $(CFLAGS) -c encode.c

decode.o: decode.c
	$(CC) $(CFLAGS) -c decode.c

lzss: $(OBJECTS)
	$(CC) $(LDFLAGS) -o lzss $(OBJECTS)

clean:
	rm -f *.o
	rm -f *~
	rm -f lzss
	rm -rf build/*
