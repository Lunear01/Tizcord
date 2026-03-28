/* tizcord server methods */
#include "tizcord_server.h"

void handle_server_packet(ServerContext* ctx, int client_fd, TizcordPacket* packet){
	switch (packet->payload.server.action) {
		case SERVER_JOIN:
			printf("[Server] Handling SERVER_JOIN action\n");
			break;
		case SERVER_LEAVE:
			printf("[Server] Handling SERVER_LEAVE action\n");
			break;
		case SERVER_CREATE:
			printf("[Server] Handling SERVER_CREATE action\n");
			break;
		case SERVER_DELETE:
			printf("[Server] Handling SERVER_DELETE action\n");
			break;
		case SERVER_EDIT:
			printf("[Server] Handling SERVER_EDIT action\n");
			break;
		case SERVER_LIST:
			printf("[Server] Handling SERVER_LIST action\n");
			break;
		case SERVER_LIST_CHANNELS:
			printf("[Server] Handling SERVER_LIST_CHANNELS action\n");
			break;
		case SERVER_LIST_MEMBERS:
			printf("[Server] Handling SERVER_LIST_MEMBERS action\n");
			break;
		case SERVER_LIST_JOINED:
			printf("[Server] Handling SERVER_LIST_JOINED action\n");
			break;
		case SERVER_KICK_MEMBER:
			printf("[Server] Handling SERVER_KICK_MEMBER action\n");
			break;
		default:
			printf("[Server] Unknown SERVER action %d received!\n", packet->payload.server.action);
			break;
	}
}

void join_tizcord_server(ServerContext* ctx, int client_index,  sqlite3_int64 server_id);
void leave_tizcord_server(ServerContext* ctx, int client_index, sqlite3_int64 server_id);
void create_tizcord_server(ServerContext* ctx, const char* server_name, int creator_fd);
void delete_tizcord_server(ServerContext* ctx, const char* server_id);
void list_tizcord_servers(ServerContext* ctx, const char* server_name);
void list_channels(ServerContext* ctx, int client_fd, sqlite3_int64 server_id);
void list_members(ServerContext* ctx, int client_fd, sqlite3_int64 server_id);
void list_joined_servers(ServerContext* ctx, int client_fd);