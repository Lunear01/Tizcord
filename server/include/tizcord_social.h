/* user social operations */
#ifndef SOCIAL_H

#define SOCIAL_H
#include "server.h"

void handle_social_packet(ServerContext *ctx, ClientNode *client, TizcordPacket *packet);

#endif