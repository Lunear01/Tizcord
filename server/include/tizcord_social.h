/* user social operations */
#ifndef SOCIAL_H

#define SOCIAL_H
#include "server.h"

void send_friend_request(ServerContext* ctx, int sender_index, const char* recipient_username);
void accept_friend_request(ServerContext* ctx, int recipient_index, const char* sender_username);
void list_friends(ServerContext* ctx, int client_index);
void remove_friend(ServerContext* ctx, int client_index, const char* friend_username);
void check_online_users(ServerContext* ctx, int client_index);
void update_user_status(ServerContext* ctx, int client_index, UserStatus new_status);
void handle_social_packet(ServerContext *ctx, ClientNode *client, TizcordPacket *packet);

#endif