/* Server operations */
#include <stdint.h>
#include "server.h"

void list_tizcord_servers(ServerContext* ctx, const char* server_name);
void join_server(ServerContext* ctx, int client_index,  uint64_t server_id);
void leave_server(ServerContext* ctx, int client_index, uint64_t server_id);
void create_tizcord_server(ServerContext* ctx, const char* server_name, int creator_fd);
void delete_tizcord_server(ServerContext* ctx, const char* server_id);