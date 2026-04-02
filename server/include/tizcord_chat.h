#ifndef CHAT_H
#define CHAT_H

#include "server.h"
#include "protocol.h"

static void message_history_cb(int64_t msg_id, const char* username, const char* content, int64_t timestamp, void* userdata);
void channel_broadcast(ServerContext *ctx, sqlite3_int64 channel_id, const char *packet, size_t packet_size, int sender_fd);
void handle_channel_message(ServerContext *ctx, TizcordPacket *packet, int sender_fd);
void handle_chat_packet(ServerContext *ctx, TizcordPacket *packet, int sender_fd);
void handle_private_message(ServerContext *ctx, TizcordPacket *packet, int sender_fd);

#endif