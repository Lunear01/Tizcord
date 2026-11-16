CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I../shared
LDFLAGS = -lncurses -lsqlite3 -luuid -lcrypt -lssl -lcrypto

all: server

server: main.o server.o auth.o db.o tizcord_chat.o tizcord_server.o tizcord_social.o client_helper.o ../shared/packet_helper.o ../shared/tls.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

../shared/%.o: ../shared/%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

db:
	sqlite3 tizcord.db < ../db/schema.sql

clean:
	rm -f *.o server client ui