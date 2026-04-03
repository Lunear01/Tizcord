#include "protocol.h"

TizcordPacket create_base_packet(PacketType type);
int send_full_packet(int fd, const TizcordPacket *packet);
int recv_full_packet(int fd, TizcordPacket *packet);
