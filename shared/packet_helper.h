#include <stdio.h>
#include <sys/types.h>
#include "protocol.h"

TizcordPacket create_base_packet(PacketType type);
int packet_send(int fd, const TizcordPacket *packet);
ssize_t packet_receive(int fd, TizcordPacket *packet);
