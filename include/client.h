#include "server.h"

typedef enum {
     STATUS_OFFLINE, 
     STATUS_ONLINE, 
     STATUS_DND 
} UserStatus;

void connect_to_server(const char *ip_address, int port);

void create_server(const char *server_name);
void leave_server(int server_id);

void create_channel(int server_id, const char *channel_name);
void delete_channel(int channel_id);

void send_friend_request(const char *target_username);
void accept_friend_request(const char *target_username);

void update_user_status(ServerContext *ctx, int client_index, UserStatus new_status);