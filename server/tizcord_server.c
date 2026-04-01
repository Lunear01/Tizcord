/* tizcord server methods */
#include "tizcord_server.h"
#include "client_helper.h"
#include "packet_helper.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char names[MAX_SERVERS][MAX_NAME_LEN];
    const char *name_ptrs[MAX_SERVERS];
    int64_t ids[MAX_SERVERS];
    int member_counts[MAX_SERVERS];
    size_t count;
    int truncated;
} ServerListAccumulator;

typedef struct {
    char names[MAX_CHANNELS][MAX_NAME_LEN];
    const char *name_ptrs[MAX_CHANNELS];
    int64_t ids[MAX_CHANNELS];
    size_t count;
    int truncated;
} ChannelListAccumulator;

typedef struct {
    char names[MAX_SERVER_MEMBERS][MAX_NAME_LEN];
    const char *name_ptrs[MAX_SERVER_MEMBERS];
    int64_t ids[MAX_SERVER_MEMBERS];
    int status_codes[MAX_SERVER_MEMBERS];
    size_t count;
    int truncated;
} MemberListAccumulator;

static void collect_server_list_item(int64_t server_id, const char *server_name, int member_count,
                                     void *userdata) {
    ServerListAccumulator *acc = (ServerListAccumulator *)userdata;
    if (acc == NULL) {
        return;
    }

    if (acc->count >= MAX_SERVERS) {
        acc->truncated = 1;
        return;
    }

    acc->ids[acc->count] = server_id;
    acc->member_counts[acc->count] = member_count;
    strncpy(acc->names[acc->count], server_name != NULL ? server_name : "",
            MAX_NAME_LEN - 1);
    acc->names[acc->count][MAX_NAME_LEN - 1] = '\0';
    acc->name_ptrs[acc->count] = acc->names[acc->count];
    acc->count++;
}

static void collect_channel_list_item(int64_t channel_id,
                                      const char *channel_name,
                                      void *userdata) {
    ChannelListAccumulator *acc = (ChannelListAccumulator *)userdata;
    if (acc == NULL) {
        return;
    }

    if (acc->count >= MAX_CHANNELS) {
        acc->truncated = 1;
        return;
    }

    acc->ids[acc->count] = channel_id;
    strncpy(acc->names[acc->count], channel_name != NULL ? channel_name : "",
            MAX_NAME_LEN - 1);
    acc->names[acc->count][MAX_NAME_LEN - 1] = '\0';
    acc->name_ptrs[acc->count] = acc->names[acc->count];
    acc->count++;
}

static void collect_member_list_item(int64_t user_id, const char *username,
                                     int is_admin, void *userdata) {
    MemberListAccumulator *acc = (MemberListAccumulator *)userdata;
    if (acc == NULL) {
        return;
    }

    if (acc->count >= MAX_SERVER_MEMBERS) {
        acc->truncated = 1;
        return;
    }

    acc->ids[acc->count] = user_id;
    acc->status_codes[acc->count] = is_admin;

    strncpy(acc->names[acc->count], username != NULL ? username : "",
            MAX_NAME_LEN - 1);
    acc->names[acc->count][MAX_NAME_LEN - 1] = '\0';
    acc->name_ptrs[acc->count] = acc->names[acc->count];
    acc->count++;
}

void handle_server_packet(ServerContext *ctx, ClientNode *client,
                          TizcordPacket *packet) {
    if (ctx == NULL || client == NULL || packet == NULL) {
        fprintf(stderr, "[Server] Invalid PACKET_SERVER packet context\n");
        return;
    }

    int64_t server_id = (sqlite3_int64)packet->payload.server.server_id;
    int64_t target_user_id =
        (sqlite3_int64)packet->payload.server.target_user_id;

    switch (packet->payload.server.action) {
    case SERVER_JOIN:
        join_tizcord_server(ctx, client, server_id);
        break;
    case SERVER_LEAVE:
        leave_tizcord_server(ctx, client, server_id);
        break;
    case SERVER_CREATE:
        create_tizcord_server(ctx, client, packet->payload.server.server_name);
        break;
    case SERVER_DELETE:
        delete_tizcord_server(ctx, client, server_id);
        break;
    case SERVER_EDIT:
        edit_tizcord_server(ctx, client, server_id,
                            packet->payload.server.server_name);
        break;
    case SERVER_LIST:
        list_tizcord_servers(ctx, client);
        break;
    case SERVER_LIST_CHANNELS:
        list_tizcord_channels(ctx, client, server_id);
        break;
    case SERVER_LIST_MEMBERS:
        list_members(ctx, client, server_id);
        break;
    case SERVER_LIST_JOINED:
        list_joined_servers(ctx, client);
        break;
    case SERVER_KICK_MEMBER:
        kick_server_member(ctx, client, server_id, target_user_id);
        break;
    default:
        printf("[Server] Unknown PACKET_SERVER action %d received!\n",
               packet->payload.server.action);
        send_action_response(client->socket_fd, PACKET_SERVER,
                             packet->payload.server.action, RESP_ERR_INVALID,
                             NULL);
        break;
    }
}

void join_tizcord_server(ServerContext *ctx, ClientNode *client,
                         int64_t server_id) {
    if (ctx == NULL || ctx->db == NULL || client == NULL || server_id <= 0) {
        fprintf(stderr, "[Server] Invalid join request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_JOIN,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        fprintf(stderr,
                "[Server] Unauthenticated client attempted SERVER_JOIN\n");
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_JOIN,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_join_server(ctx->db, server_id, client->id, false) == 0) {
        printf("[Server] User id=%lld joined server id=%lld\n", (long long)client->id,
               (long long)server_id);
        if (send_action_response(client->socket_fd, PACKET_SERVER, SERVER_JOIN,
                                 RESP_OK, NULL) != 0) {
            fprintf(stderr,
                    "[Server] Failed to send join response to client id=%lld\n",
                    (long long)client->id);
        }
    } else {
        fprintf(stderr, "[Server] Failed to join server id=%lld\n", (long long)server_id);
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_JOIN,
                             RESP_ERR_DB, NULL);
    }
}

void leave_tizcord_server(ServerContext *ctx, ClientNode *client,
                          int64_t server_id) {
    if (ctx == NULL || ctx->db == NULL || client == NULL || server_id <= 0) {
        fprintf(stderr, "[Server] Invalid leave request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LEAVE,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        fprintf(stderr,
                "[Server] Unauthenticated client attempted SERVER_LEAVE\n");
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LEAVE,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_leave_server(ctx->db, server_id, client->id) == 0) {
        printf("[Server] User id=%lld left server id=%lld\n", (long long)client->id,
               (long long)server_id);
        if (send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LEAVE,
                                 RESP_OK, NULL) != 0) {
            fprintf(
                stderr,
                "[Server] Failed to send leave response to client id=%lld\n",
                client->id);
        }
    } else {
        fprintf(stderr, "[Server] Failed to leave server id=%lld\n", (long long)server_id);
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LEAVE,
                             RESP_ERR_DB, NULL);
    }
}

void create_tizcord_server(ServerContext *ctx, ClientNode *client,
                           const char *server_name) {
    if (ctx == NULL || ctx->db == NULL || client == NULL ||
        server_name == NULL || server_name[0] == '\0') {
        fprintf(stderr, "[Server] Invalid create server request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_CREATE,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        fprintf(stderr,
                "[Server] Unauthenticated client attempted SERVER_CREATE\n");
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_CREATE,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_create_server(ctx->db, server_name, client->id) == 0) {
        printf("[Server] User id=%lld created server '%s'\n", (long long)client->id,
               server_name);
        if (send_action_response(client->socket_fd, PACKET_SERVER, SERVER_CREATE,
                                 RESP_OK, NULL) != 0) {
            fprintf(
                stderr,
                "[Server] Failed to send create response to client id=%lld\n",
                (long long)client->id);
        }
    } else {
        fprintf(stderr, "[Server] Failed to create server '%s'\n", server_name);
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_CREATE,
                             RESP_ERR_DB, NULL);
    }
}

void get_tizcord_server_info(ServerContext *ctx, ClientNode *client,
                             int64_t server_id) {
    list_tizcord_channels(ctx, client, server_id);
    list_members(ctx, client, server_id);
}

void delete_tizcord_server(ServerContext *ctx, ClientNode *client,
                           int64_t server_id) {
    if (ctx == NULL || ctx->db == NULL || client == NULL || server_id <= 0) {
        fprintf(stderr, "[Server] Invalid delete server request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_DELETE,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        fprintf(stderr,
                "[Server] Unauthenticated client attempted SERVER_DELETE\n");
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_DELETE,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    int is_admin = 0;
    if (db_user_is_server_admin(ctx->db, server_id, client->id, &is_admin) != 0 || !is_admin) {
        fprintf(stderr, "[Server] Unauthorized server delete attempt by id=%lld\n", (long long)client->id);
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_DELETE, RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_delete_server(ctx->db, server_id, client->id) == 0) {
        printf("[Server] User id=%lld deleted server id=%lld\n", (long long)client->id,
               (long long)server_id);
        if (send_action_response(client->socket_fd, PACKET_SERVER, SERVER_DELETE,
                                 RESP_OK, NULL) != 0) {
            fprintf(
                stderr,
                "[Server] Failed to send delete response to client id=%lld\n",
                (long long)client->id);
        }
    } else {
        fprintf(stderr, "[Server] Failed to delete server id=%lld\n",
                (long long)server_id);
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_DELETE,
                             RESP_ERR_DB, NULL);
    }
}

void edit_tizcord_server(ServerContext *ctx, ClientNode *client,
                         int64_t server_id, const char *new_name) {
    if (ctx == NULL || ctx->db == NULL || client == NULL || server_id <= 0 ||
        new_name == NULL || new_name[0] == '\0') {
        fprintf(stderr, "[Server] Invalid edit server request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_EDIT,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        fprintf(stderr,
                "[Server] Unauthenticated client attempted SERVER_EDIT\n");
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_EDIT,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_edit_server(ctx->db, server_id, client->id, new_name) == 0) {
        printf("[Server] User id=%lld edited server id=%lld\n", (long long)client->id,
               (long long)server_id);
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_EDIT, RESP_OK,
                             NULL);
    } else {
        fprintf(stderr, "[Server] Failed to edit server id=%lld\n", (long long)server_id);
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_EDIT,
                             RESP_ERR_DB, NULL);
    }
}

void list_tizcord_servers(ServerContext *ctx, ClientNode *client) {
    ServerListAccumulator acc = {0};

    if (ctx == NULL || ctx->db == NULL || client == NULL) {
        fprintf(stderr, "[Server] Invalid list servers request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_list_servers(ctx->db, collect_server_list_item, &acc) != 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST,
                             RESP_ERR_DB, NULL);
        return;
    }

    if (send_list_response(client->socket_fd, PACKET_SERVER, SERVER_LIST,
                           acc.name_ptrs, acc.ids, acc.member_counts, acc.count) != 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST,
                             RESP_ERR_INTERNAL, NULL);
    }
}

void list_tizcord_channels(ServerContext *ctx, ClientNode *client, int64_t server_id) {
    ChannelListAccumulator acc = {0};

    if (ctx == NULL || ctx->db == NULL || client == NULL || server_id <= 0) {
        fprintf(stderr, "[Server] Invalid list channels request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER,
                                 SERVER_LIST_CHANNELS, RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_CHANNELS,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_list_server_channels(ctx->db, server_id, collect_channel_list_item,
                                &acc) != 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_CHANNELS,
                             RESP_ERR_DB, NULL);
        return;
    }

    if (send_list_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_CHANNELS,
                           acc.name_ptrs, acc.ids, NULL, acc.count) != 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_CHANNELS,
                             RESP_ERR_INTERNAL, NULL);
    }
}

void list_members(ServerContext *ctx, ClientNode *client, int64_t server_id) {
    MemberListAccumulator acc = {0};

    if (ctx == NULL || ctx->db == NULL || client == NULL || server_id <= 0) {
        fprintf(stderr, "[Server] Invalid list members request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_MEMBERS,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_MEMBERS,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_list_server_members(ctx->db, server_id, collect_member_list_item,
                               &acc) != 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_MEMBERS,
                             RESP_ERR_DB, NULL);
        return;
    }

    if (send_list_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_MEMBERS,
                           acc.name_ptrs, acc.ids, acc.status_codes, acc.count) != 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_MEMBERS,
                             RESP_ERR_INTERNAL, NULL);
    }
}

void list_joined_servers(ServerContext *ctx, ClientNode *client) {
    ServerListAccumulator acc = {0};

    if (ctx == NULL || ctx->db == NULL || client == NULL) {
        fprintf(stderr, "[Server] Invalid list joined servers request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_JOINED,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_JOINED,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_list_joined_servers(ctx->db, client->id, collect_server_list_item,
                               &acc) != 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_JOINED,
                             RESP_ERR_DB, NULL);
        return;
    }

    if (send_list_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_JOINED,
                           acc.name_ptrs, acc.ids, acc.member_counts, acc.count) != 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_LIST_JOINED,
                             RESP_ERR_INTERNAL, NULL);
    }
}

void kick_server_member(ServerContext *ctx, ClientNode *client,
                        int64_t server_id, int64_t target_user_id) {
    int is_admin = 0;

    if (ctx == NULL || ctx->db == NULL || client == NULL || server_id <= 0 ||
        target_user_id <= 0) {
        fprintf(stderr, "[Server] Invalid kick member request\n");
        if (client != NULL) {
            send_action_response(client->socket_fd, PACKET_SERVER, SERVER_KICK_MEMBER,
                                 RESP_ERR_INVALID, NULL);
        }
        return;
    }

    if (!client->is_authenticated) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_KICK_MEMBER,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_user_is_server_admin(ctx->db, server_id, client->id, &is_admin) !=
            0 ||
        !is_admin) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_KICK_MEMBER,
                             RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    if (db_kick_server_member(ctx->db, server_id, target_user_id) == 0) {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_KICK_MEMBER,
                             RESP_OK, NULL);
    } else {
        send_action_response(client->socket_fd, PACKET_SERVER, SERVER_KICK_MEMBER,
                             RESP_ERR_DB, NULL);
    }
}
