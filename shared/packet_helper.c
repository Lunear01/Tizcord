#include <stdio.h>
#include <string.h>
#include <time.h>
#include "packet_helper.h"
#include "protocol.h"
// Helper function to initialize a base packet
TizcordPacket create_base_packet(PacketType type) {
    TizcordPacket packet;
    memset(&packet, 0, sizeof(TizcordPacket));
    packet.type = type;
    packet.timestamp = time(NULL);
    return packet;
}