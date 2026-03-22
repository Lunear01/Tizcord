#include "server.h"
#include "protocol.h"

void handle_chat_packet(ServerContext *ctx, TizcordPacket *packet, int sender_fd);
void send_private_message(ServerContext *ctx, TizcordPacket *packet);
void send_to_channel(ServerContext *ctx, TizcordPacket *packet);
void delete_message(ServerContext *ctx, TizcordPacket *packet);
void edit_messsage(ServerContext *ctx, TizcordPacket *packet);

