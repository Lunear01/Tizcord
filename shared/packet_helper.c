#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
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

int send_full_packet(int fd, const TizcordPacket *packet) {
    if (fd < 0 || packet == NULL) {
        errno = EINVAL;
        return -1;
    }

    size_t total_sent = 0;
    const char *buffer = (const char *)packet;

    while (total_sent < sizeof(TizcordPacket)) {
        ssize_t bytes_sent = send(fd,
                                  buffer + total_sent,
                                  sizeof(TizcordPacket) - total_sent,
                                  MSG_NOSIGNAL);
        if (bytes_sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (bytes_sent == 0) {
            errno = EPIPE;
            return -1;
        }

        total_sent += (size_t)bytes_sent;
    }

    return 0;
}

int recv_full_packet(int fd, TizcordPacket *packet) {
    if (fd < 0 || packet == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(packet, 0, sizeof(TizcordPacket));

    size_t total_read = 0;
    char *buffer = (char *)packet;

    while (total_read < sizeof(TizcordPacket)) {
        ssize_t bytes_read = recv(fd,
                                  buffer + total_read,
                                  sizeof(TizcordPacket) - total_read,
                                  0);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (bytes_read == 0) {
            if (total_read == 0) {
                return 0;
            }
            errno = ECONNRESET;
            return -1;
        }

        total_read += (size_t)bytes_read;
    }

    return 1;
}
