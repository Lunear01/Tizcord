/* Server operations */
#include <stdint.h>
#include "server.h"

void handle_server_packet(ServerContext* ctx, ClientNode *client, TizcordPacket *packet);
void join_tizcord_server(ServerContext* ctx, ClientNode *client,  int64_t server_id);
void leave_tizcord_server(ServerContext* ctx, ClientNode *client, int64_t server_id);
void create_tizcord_server(ServerContext* ctx, ClientNode *client, const char* server_name);
void get_tizcord_server_info(ServerContext* ctx, ClientNode *client, int64_t server_id);
void delete_tizcord_server(ServerContext* ctx, ClientNode *client, int64_t server_id);
void edit_tizcord_server(ServerContext* ctx, ClientNode *client, int64_t server_id, const char* new_name);
void list_tizcord_servers(ServerContext* ctx, ClientNode *client);
void list_channels(ServerContext* ctx, ClientNode *client, int64_t server_id);
void list_members(ServerContext* ctx, ClientNode *client, int64_t server_id);
void list_joined_servers(ServerContext* ctx, ClientNode *client);
void kick_server_member(ServerContext* ctx, ClientNode *client, int64_t server_id, int64_t target_user_id);