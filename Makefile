CC=gcc
CFLAGS=-Wall -Wextra -Wwrite-strings -O -g -lsensors

all: eeepc-fanctld

eeepc-fanctld: eeepc-fanctld.c
	$(CC) $< $(CFLAGS) -o $@

clean:
	rm -f *.o eeepc-fanctld
