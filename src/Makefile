CC = gcc
CFLAGS = -Wall -Wextra -lncurses -I../include

all: server client

server: main.o server.o
	$(CC) $(CFLAGS) -o $@ $^

client: client.o
	$(CC) $(CFLAGS) -o $@ $^

ui: ui.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o server client