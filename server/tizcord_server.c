/* tizcord server methods */
#include "tizcord_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static int find_client_index_by_fd(ServerContext* ctx, int client_fd) {
	if (ctx == NULL) return -1;

	for (int i = 0; i < ctx->client_count; i++) {
		if (ctx->clients[i].socket_fd == client_fd) {
			return i;
		}
	}

	return -1;
}

static void list_server_cb(sqlite3_int64 server_id, const char* server_name, void* userdata) {
	(void)userdata;
	printf("[Server] Server: id=%lld name=%s\n", (int64_t)server_id, server_name);
}

static void list_channel_cb(sqlite3_int64 channel_id, const char* channel_name, void* userdata) {
	(void)userdata;
	printf("[Server] Channel: id=%lld name=%s\n", (int64_t)channel_id, channel_name);
}

static void list_member_cb(sqlite3_int64 user_id, const char* username, int is_admin, void* userdata) {
	(void)userdata;
	printf("[Server] Member: id=%lld username=%s admin=%d\n", (int64_t)user_id, username, is_admin);
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

	sqlite3_int64 server_id = (sqlite3_int64)packet->payload.server.server_id;
	sqlite3_int64 target_user_id = (sqlite3_int64)packet->payload.server.target_user_id;

	switch (packet->payload.server.action) {
		case SERVER_JOIN:
			join_tizcord_server(ctx, client_index, server_id);
			break;
		case SERVER_LEAVE:
			leave_tizcord_server(ctx, client_index, server_id);
			break;
		case SERVER_CREATE:
			create_tizcord_server(ctx, packet->payload.server.server_name, client_fd);
			break;
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
			list_tizcord_servers(ctx, packet->payload.server.server_name);
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

void join_tizcord_server(ServerContext* ctx, int client_index, sqlite3_int64 server_id) {
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

void leave_tizcord_server(ServerContext* ctx, int client_index, sqlite3_int64 server_id) {
	if (ctx == NULL || ctx->db == NULL || client_index < 0 || client_index >= ctx->client_count) {
		fprintf(stderr, "[Server] Invalid leave request\n");
		return;
	}

	ClientNode* client = &ctx->clients[client_index];
	if (!client->is_authenticated) {
		fprintf(stderr, "[Server] Unauthenticated client attempted SERVER_LEAVE\n");
		return;
	}

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

	sqlite3_int64 new_server_id = 0;
	if (db_create_server(ctx->db, server_name, creator->id, &new_server_id) == 0) {
		printf("[Server] Created server id=%lld name=%s\n", (int64_t)new_server_id, server_name);
	} else {
		fprintf(stderr, "[Server] Failed to create server name=%s\n", server_name);
	}
}

void get_tizcord_server_info(ServerContext* ctx, int client_fd, sqlite3_int64 server_id) {
	list_channels(ctx, client_fd, server_id);
	list_members(ctx, client_fd, server_id);
}

void delete_tizcord_server(ServerContext* ctx, const char* server_id) {
	if (ctx == NULL || server_id == NULL) {
		fprintf(stderr, "[Server] Invalid delete server request\n");
		return;
	}

	fprintf(stderr,
		"[Server] delete_tizcord_server(%s) called without requester context; "
		"use SERVER_DELETE packet handling path instead\n",
		server_id);
}

void list_tizcord_servers(ServerContext* ctx, const char* server_name) {
	if (ctx == NULL || ctx->db == NULL) {
		fprintf(stderr, "[Server] Invalid list servers request\n");
		return;
	}

	if (server_name != NULL && server_name[0] != '\0') {
		sqlite3_int64 server_id = 0;
		if (db_get_server(ctx->db, server_name, &server_id) == 0) {
			printf("[Server] Server: id=%lld name=%s\n", (int64_t)server_id, server_name);
		} else {
			fprintf(stderr, "[Server] Server not found: %s\n", server_name);
		}
		return;
	}

	if (db_list_servers(ctx->db, list_server_cb, NULL) != 0) {
		fprintf(stderr, "[Server] Failed to list servers\n");
	}
}

void list_channels(ServerContext* ctx, int client_fd, sqlite3_int64 server_id) {
	(void)client_fd;

	if (ctx == NULL || ctx->db == NULL) {
		fprintf(stderr, "[Server] Invalid list channels request\n");
		return;
	}

	if (db_list_server_channels(ctx->db, server_id, list_channel_cb, NULL) != 0) {
		fprintf(stderr, "[Server] Failed to list channels for server id=%lld\n", (int64_t)server_id);
	}
}

void list_members(ServerContext* ctx, int client_fd, sqlite3_int64 server_id) {
	(void)client_fd;

	if (ctx == NULL || ctx->db == NULL) {
		fprintf(stderr, "[Server] Invalid list members request\n");
		return;
	}

	if (db_list_server_members(ctx->db, server_id, list_member_cb, NULL) != 0) {
		fprintf(stderr, "[Server] Failed to list members for server id=%lld\n", (int64_t)server_id);
	}
}

void list_joined_servers(ServerContext* ctx, int client_fd) {
	if (ctx == NULL || ctx->db == NULL) {
		fprintf(stderr, "[Server] Invalid list joined servers request\n");
		return;
	}

	int client_index = find_client_index_by_fd(ctx, client_fd);
	if (client_index < 0) {
		fprintf(stderr, "[Server] Could not resolve client index for fd=%d\n", client_fd);
		return;
	}

	ClientNode* client = &ctx->clients[client_index];
	if (!client->is_authenticated) {
		fprintf(stderr, "[Server] Unauthenticated client attempted SERVER_LIST_JOINED\n");
		return;
	}

	if (db_list_joined_servers(ctx->db, client->id, list_server_cb, NULL) != 0) {
		fprintf(stderr, "[Server] Failed to list joined servers for user id=%lld\n",
			(int64_t)client->id);
	}
}