CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I../shared
LDFLAGS = -lncurses -lpthread

SHARED_OBJ = ../shared/packet_helper.o

all: client

client: client_main.o client.o ui.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

../shared/packet_helper.o: ../shared/packet_helper.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o client ../shared/*.o