#include "server.h"
#include "protocol.h"

void handle_chat_packet(ServerContext *ctx, TizcordPacket *packet, int sender_fd);
void send_private_message(ServerContext *ctx, TizcordPacket *packet);
void send_to_group(ServerContext *ctx, TizcordPacket *packet);
void update_user_status(ServerContext *ctx, int client_index, UserStatus new_status);
