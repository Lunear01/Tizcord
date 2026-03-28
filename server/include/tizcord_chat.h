#ifndef CHAT_H
#define CHAT_H

#include "server.h"
#include "protocol.h"

void handle_chat_packet(ServerContext *ctx, TizcordPacket *packet, int sender_fd);
void handle_private_message(ServerContext *ctx, TizcordPacket *packet, int sender_fd);

#endif