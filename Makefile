all: randpop

CFLAGS = -O2 -Wall -g
LDFLAGS = -pthread

randpop: randpop.c
	cc -o randpop randpop.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f randpop

