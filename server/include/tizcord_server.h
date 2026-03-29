/* Server operations */
#include <stdint.h>
#include "server.h"

typedef struct {
	int client_fd;
	int send_failed;
} ServerReplyCtx;

typedef struct {
	int client_fd;
	int send_failed;
} JoinedServerReplyCtx;

void handle_server_packet(ServerContext* ctx, int client_fd, TizcordPacket* packet);
void join_tizcord_server(ServerContext* ctx, int client_index,  int64_t server_id);
void leave_tizcord_server(ServerContext* ctx, int client_index, int64_t server_id);
void create_tizcord_server(ServerContext* ctx, const char* server_name, int creator_fd);
void get_tizcord_server_info(ServerContext* ctx, int client_fd, int64_t server_id);
void delete_tizcord_server(ServerContext* ctx, int64_t server_id);
void list_tizcord_servers(ServerContext* ctx);
void list_channels(ServerContext* ctx, int client_fd, int64_t server_id);
void list_members(ServerContext* ctx, int client_fd, int64_t server_id);
void list_joined_servers(ServerContext* ctx, int client_fd);