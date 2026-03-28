CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I../shared
LDFLAGS = -lncurses -lpthread

all: client

client: client_main.o client.o ui.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o client