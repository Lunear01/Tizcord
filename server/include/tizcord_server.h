/* Server operations */
#include <sqlite3.h>
#include "server.h"

void handle_server_packet(ServerContext* ctx, int client_fd, TizcordPacket* packet);
void join_tizcord_server(ServerContext* ctx, int client_index,  sqlite3_int64 server_id);
void leave_tizcord_server(ServerContext* ctx, int client_index, sqlite3_int64 server_id);
void create_tizcord_server(ServerContext* ctx, const char* server_name, int creator_fd);
void delete_tizcord_server(ServerContext* ctx, const char* server_id);
void list_tizcord_servers(ServerContext* ctx, const char* server_name);
void list_channels(ServerContext* ctx, int client_fd, sqlite3_int64 server_id);
void list_members(ServerContext* ctx, int client_fd, sqlite3_int64 server_id);
void list_joined_servers(ServerContext* ctx, int client_fd);