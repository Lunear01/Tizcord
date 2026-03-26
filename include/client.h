#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"

void connect_to_server(const char *ip_address, int port);

int send_register(const char *username, const char *password);

void create_server(const char *server_name);
void leave_server(int server_id);

void create_channel(int server_id, const char *channel_name);
void delete_channel(int channel_id);

void send_friend_request(const char *target_username);
void accept_friend_request(const char *target_username);
void update_user_status(ServerContext *ctx, int client_index, UserStatus new_status);

#endif