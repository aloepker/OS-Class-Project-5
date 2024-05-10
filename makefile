CC = gcc
CFLAGS = -g -Wall -Wshadow


all: worker oss

worker: worker.c
	$(CC) $(CFLAGS) -o worker worker.c

occ: oss.c
	$(CC) $(CFLAGS)

clean:
	rm -f worker oss
