CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I../shared
LDFLAGS = -lncurses -lssl -lcrypto

SHARED_OBJ = ../shared/packet_helper.o ../shared/tls.o

all: client

client: client_main.o client.o ui.o $(SHARED_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

../shared/%.o: ../shared/%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o client ../shared/*.o