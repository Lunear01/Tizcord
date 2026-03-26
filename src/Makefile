CC = gcc
CFLAGS = -Wall -Wextra -I../include
LDFLAGS = -lncurses -lsqlite3 -luuid -lcrypt

all: server client

server: server_main.o server.o auth.o db.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: client_main.o client.o ui.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

ui: ui.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

db:
	sqlite3 tizcord.db < ../migrations/001_init.sql

clean:
	rm -f *.o server client ui