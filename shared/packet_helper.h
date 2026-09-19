<<<<<<< HEAD
#include <stdio.h>
#include <sys/types.h>
#include "protocol.h"

TizcordPacket create_base_packet(PacketType type);
int packet_send(int fd, const TizcordPacket *packet);
ssize_t packet_receive(int fd, TizcordPacket *packet);
=======
#include "protocol.h"

TizcordPacket create_base_packet(PacketType type);
int send_full_packet(int fd, const TizcordPacket *packet);
int recv_full_packet(int fd, TizcordPacket *packet);
>>>>>>> 2069d63712814baa0e39429d04fa64de6d8e609a
