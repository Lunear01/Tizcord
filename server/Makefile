CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I../shared
LDFLAGS = -lncurses -lsqlite3 -luuid -lcrypt

all: server

server: server_main.o server.o auth.o db.o tizcord_chat.o tizcord_server.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

db:
	sqlite3 tizcord.db < ../migrations/001_init.sql

clean:
	rm -f *.o server client ui