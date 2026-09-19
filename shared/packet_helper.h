#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "protocol.h"

TizcordPacket create_base_packet(PacketType type);
int packet_send(int fd, const TizcordPacket *packet);
ssize_t packet_receive(int fd, TizcordPacket *packet);
int packet_receive_partial(int fd, uint8_t *buffer, size_t *offset,
						   TizcordPacket *packet);
