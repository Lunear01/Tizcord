#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
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

_Static_assert(sizeof(int) == 4 && sizeof(PacketType) == 4 &&
               sizeof(ListFrameType) == 4 && sizeof(SystemAction) == 4 &&
               sizeof(AuthAction) == 4 && sizeof(DMAction) == 4 &&
               sizeof(ServerAction) == 4 && sizeof(ChannelAction) == 4 &&
               sizeof(SocialAction) == 4 && sizeof(UserStatus) == 4,
               "packet integer and enum fields must be 32 bits");

static uint64_t network64(uint64_t value) {
    const uint16_t one = 1;
    if (*(const uint8_t *)&one == 0) return value;
    return ((uint64_t)htonl((uint32_t)value) << 32) |
           htonl((uint32_t)(value >> 32));
}

/* Conversion is its own inverse: htonl and ntohl both swap on little endian. */
#define SWAP32(field) ((field) = (__typeof__(field))htonl((uint32_t)(field)))
#define SWAP64(field) ((field) = (int64_t)network64((uint64_t)(field)))

static void swap_packet(TizcordPacket *packet, PacketType type) {
    SWAP32(packet->type);
    SWAP64(packet->sender_id);
    SWAP32(packet->list_id);
    SWAP32(packet->list_index);
    SWAP32(packet->list_total);
    SWAP32(packet->list_frame);
    SWAP64(packet->timestamp);
    switch (type) {
    case PACKET_AUTH:
        SWAP32(packet->payload.auth.action);
        SWAP32(packet->payload.auth.status_code);
        break;
    case PACKET_DM:
        SWAP32(packet->payload.dm.action);
        SWAP32(packet->payload.dm.status_code);
        SWAP32(packet->payload.dm.recipient_id);
        SWAP64(packet->payload.dm.message_id);
        break;
    case PACKET_SERVER:
        SWAP32(packet->payload.server.action);
        SWAP64(packet->payload.server.server_id);
        SWAP64(packet->payload.server.target_user_id);
        SWAP32(packet->payload.server.status_code);
        SWAP32(packet->payload.server.member_count);
        break;
    case PACKET_CHANNEL:
        SWAP32(packet->payload.channel.action);
        SWAP64(packet->payload.channel.server_id);
        SWAP32(packet->payload.channel.status_code);
        SWAP64(packet->payload.channel.channel_id);
        SWAP64(packet->payload.channel.message_id);
        break;
    case PACKET_SOCIAL:
        SWAP32(packet->payload.social.status);
        SWAP32(packet->payload.social.action);
        SWAP32(packet->payload.social.status_code);
        SWAP64(packet->payload.social.target_user_id);
        break;
    default:
        SWAP32(packet->payload.system.action);
        SWAP32(packet->payload.system.status_code);
        break;
    }
}

int packet_send(int fd, const TizcordPacket *packet) {
    TizcordPacket wire = *packet;
    swap_packet(&wire, packet->type);
    const uint8_t *bytes = (const uint8_t *)&wire;
    size_t offset = 0;
    while (offset < sizeof(wire)) {
        ssize_t count = send(fd, bytes + offset, sizeof(wire) - offset, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        offset += (size_t)count;
    }
    return 0;
}

ssize_t packet_receive(int fd, TizcordPacket *packet) {
    uint8_t *bytes = (uint8_t *)packet;
    size_t offset = 0;
    while (offset < sizeof(*packet)) {
        ssize_t count = recv(fd, bytes + offset, sizeof(*packet) - offset, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return count == 0 && offset == 0 ? 0 : -1;
        offset += (size_t)count;
    }
    PacketType type = (PacketType)ntohl((uint32_t)packet->type);
    if (type < PACKET_AUTH || type > PACKET_SOCIAL) return -1;
    swap_packet(packet, type);
    return (ssize_t)offset;
}
