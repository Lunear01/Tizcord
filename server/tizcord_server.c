/* tizcord server methods */
#include "tizcord_server.h"
#include "packet_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static int find_client_index_by_fd(ServerContext* ctx, int client_fd) {
	if (ctx == NULL) return -1;

	for (int i = 0; i < ctx->client_count; i++) {
		if (ctx->clients[i].socket_fd == client_fd) {
			return i;
		}
	}

	return -1;
}

void handle_server_packet(ServerContext* ctx, int client_fd, TizcordPacket* packet){
	if (ctx == NULL || packet == NULL) {
		fprintf(stderr, "[Server] Invalid SERVER packet context\n");
		return;
	}

	int client_index = find_client_index_by_fd(ctx, client_fd);
	if (client_index < 0) {
		fprintf(stderr, "[Server] Could not resolve client index for fd=%d\n", client_fd);
		return;
	}

	int64_t server_id = (sqlite3_int64)packet->payload.server.server_id;
	int64_t target_user_id = (sqlite3_int64)packet->payload.server.target_user_id;

	switch (packet->payload.server.action) {
		case SERVER_JOIN:
			join_tizcord_server(ctx, client_index, server_id);
			break;
		case SERVER_LEAVE:
			leave_tizcord_server(ctx, client_index, server_id);
			break;
		case SERVER_CREATE:
			create_tizcord_server(ctx, packet->payload.server.server_name, client_fd);
		case SERVER_DELETE:
			if (db_delete_server(ctx->db, server_id, ctx->clients[client_index].id) == 0) {
				printf("[Server] Deleted server id=%lld\n", (int64_t)server_id);
			} else {
				fprintf(stderr, "[Server] Failed to delete server id=%lld\n", (int64_t)server_id);
			}
			break;
		case SERVER_EDIT:
			if (db_edit_server(ctx->db, server_id, ctx->clients[client_index].id,
					packet->payload.server.server_name) == 0) {
				printf("[Server] Renamed server id=%lld to %s\n",
					(int64_t)server_id, packet->payload.server.server_name);
			} else {
				fprintf(stderr, "[Server] Failed to rename server id=%lld\n", (int64_t)server_id);
			}
			break;
		case SERVER_LIST:
			list_tizcord_servers(ctx);
			break;
		case SERVER_LIST_CHANNELS:
			list_channels(ctx, client_fd, server_id);
			break;
		case SERVER_LIST_MEMBERS:
			list_members(ctx, client_fd, server_id);
			break;
		case SERVER_LIST_JOINED:
			list_joined_servers(ctx, client_fd);
			break;
		case SERVER_KICK_MEMBER:
			if (db_kick_server_member(ctx->db, server_id, target_user_id) == 0) {
				printf("[Server] Kicked user id=%lld from server id=%lld\n",
					(int64_t)target_user_id, (long long)server_id);
			} else {
				fprintf(stderr,
					"[Server] Failed to kick user id=%lld from server id=%lld\n",
					(int64_t)target_user_id, (long long)server_id);
			}
			break;
		default:
			printf("[Server] Unknown SERVER action %d received!\n", packet->payload.server.action);
			break;
	}
}

void join_tizcord_server(ServerContext* ctx, int client_index, int64_t server_id) {
	if (ctx == NULL || ctx->db == NULL || client_index < 0 || client_index >= ctx->client_count) {
		fprintf(stderr, "[Server] Invalid join request\n");
		return;
	}

	ClientNode* client = &ctx->clients[client_index];
	if (!client->is_authenticated) {
		fprintf(stderr, "[Server] Unauthenticated client attempted SERVER_JOIN\n");
		return;
	}

	if (db_join_server(ctx->db, server_id, client->id, false) == 0) {
		printf("[Server] User id=%lld joined server id=%lld\n",
			(int64_t)client->id, (long long)server_id);
	} else {
		fprintf(stderr, "[Server] Failed to join server id=%lld\n", (int64_t)server_id);
	}
}

void leave_tizcord_server(ServerContext* ctx, int client_index, int64_t server_id) {
	if (ctx == NULL || ctx->db == NULL || client_index < 0 || client_index >= ctx->client_count) {
		fprintf(stderr, "[Server] Invalid leave request\n");
		return;
	}

	ClientNode* client = &ctx->clients[client_index];
	if (!client->is_authenticated) {
		fprintf(stderr, "[Server] Unauthenticated client attempted SERVER_LEAVE\n");
		return;
	}

	// if server owner leaves, server gets deleted
	if (db_user_is_server_admin(ctx->db, server_id, client->id, &(int){0}) == 0) {
		db_delete_server(ctx->db, server_id, client->id);
		printf("[Server] User id=%lld was admin and deleted server id=%lld\n",
			(int64_t)client->id, (long long)server_id);
		return;
	}

	// if user is not the owner, just leave the server
	if (db_leave_server(ctx->db, server_id, client->id) == 0) {
		printf("[Server] User id=%lld left server id=%lld\n",
			(int64_t)client->id, (long long)server_id);
	} else {
		fprintf(stderr, "[Server] Failed to leave server id=%lld\n", (int64_t)server_id);
	}
}

void create_tizcord_server(ServerContext* ctx, const char* server_name, int creator_fd) {
	if (ctx == NULL || ctx->db == NULL || server_name == NULL || server_name[0] == '\0') {
		fprintf(stderr, "[Server] Invalid create server request\n");
		return;
	}

	int client_index = find_client_index_by_fd(ctx, creator_fd);
	if (client_index < 0) {
		fprintf(stderr, "[Server] Could not resolve creator fd=%d\n", creator_fd);
		return;
	}

	ClientNode* creator = &ctx->clients[client_index];
	if (!creator->is_authenticated) {
		fprintf(stderr, "[Server] Unauthenticated client attempted SERVER_CREATE\n");
		return;
	}

	int64_t new_server_id = 0;
	if (db_create_server(ctx->db, server_name, creator->id, &new_server_id) == 0) {
		printf("[Server] Created server id=%lld name=%s\n", (int64_t)new_server_id, server_name);
	} else {
		fprintf(stderr, "[Server] Failed to create server name=%s\n", server_name);
	}
}

void get_tizcord_server_info(ServerContext* ctx, int client_fd, int64_t server_id) {
	list_channels(ctx, client_fd, server_id);
	list_members(ctx, client_fd, server_id);
}

void delete_tizcord_server(ServerContext* ctx, int64_t server_id) {
	if (ctx == NULL || ctx->db == NULL || server_id == NULL) {
		fprintf(stderr, "[Server] Invalid delete server request\n");
		return;
	}
	
	if (db_user_is_server_admin(ctx->db, server_id, -1, &(int){0}) != 0) {
		fprintf(stderr, "[Server] Only server admins can delete servers id=%lld\n", server_id);
		return;
	}

	if(db_delete_server(ctx->db, server_id, -1) == 0) {
		printf("[Server] Deleted server id=%s\n", server_id);
	} else {
		fprintf(stderr, "[Server] Failed to delete server id=%lld\n", server_id);
	}
}

void server_cb(int64_t server_id, const char* server_name, void* userdata) {
	TizcordPacket packet = create_base_packet(SERVER);
	packet.payload.server.action = SERVER_LIST;
	packet.payload.server.server_id = server_id;
	strncpy(packet.payload.server.server_name, server_name, sizeof(packet.payload.server.server_name) - 1);
}

void list_tizcord_servers(ServerContext* ctx) {
	if (ctx == NULL || ctx->db == NULL) {
		fprintf(stderr, "[Server] Invalid list servers request\n");
		return;
	}

	if (db_list_servers(ctx->db, server_cb, NULL) != 0) {
		fprintf(stderr, "[Server] Failed to list servers\n");
	}
}

void list_channels(ServerContext* ctx, int client_fd, int64_t server_id) {
}

void list_members(ServerContext* ctx, int client_fd, int64_t server_id) {
}

void list_joined_servers(ServerContext* ctx, int client_fd) {
}