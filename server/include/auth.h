#ifndef AUTH_H
#define AUTH_H

#include "protocol.h"
#include "server.h"

// The network packet router
void handle_auth_packet(ServerContext *ctx, int client_fd, TizcordPacket *packet);

void login_account(ServerContext *ctx, int client_fd, TizcordPacket *packet);
void logout_account(ServerContext *ctx, int client_fd, TizcordPacket *packet);
void register_account(ServerContext *ctx, int client_fd, TizcordPacket *packet);

#endif
