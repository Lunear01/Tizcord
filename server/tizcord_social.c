#include "include/tizcord_social.h"
#include "include/db.h"
#include "../shared/packet_helper.h"
#include "include/client_helper.h"
#include "../shared/protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int client_fd;
    int32_t current_index;
    int32_t list_id;
} FriendRequestListContext;

typedef struct {
    ServerContext *srv_ctx;
    int client_fd;
    int32_t current_index;
    int32_t list_id;
} UserListContext;

static void friend_request_cb(int64_t target_user_id, const char* username, int is_incoming, void *userdata) {
    FriendRequestListContext *ctx = (FriendRequestListContext *)userdata;
    if (!ctx) return;

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_LIST_FRIENDS;
    packet.list_id = ctx->list_id;
    packet.list_index = ctx->current_index++;
    packet.list_total = -1;
    packet.list_frame = LIST_FRAME_MIDDLE;

    packet.payload.social.target_user_id = target_user_id;
    strncpy(packet.payload.social.target_username, username, MAX_NAME_LEN - 1);
    packet.payload.social.status_code = is_incoming;

    write(ctx->client_fd, &packet, sizeof(TizcordPacket));
}

static void user_list_cb(int64_t user_id, const char* username, const char* status_text, void* userdata) {
    UserListContext *ctx = (UserListContext *)userdata;
    if (!ctx) return;

    int is_online = 0;
    for (int i = 0; i < ctx->srv_ctx->client_count; i++) {
        if (ctx->srv_ctx->clients[i].socket_fd > 0 &&
            ctx->srv_ctx->clients[i].id == user_id &&
            ctx->srv_ctx->clients[i].is_authenticated) {
            is_online = 1;
            break;
        }
    }

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_LIST_USERS;
    packet.list_id = ctx->list_id;
    packet.list_index = ctx->current_index++;
    packet.list_total = -1;
    packet.list_frame = LIST_FRAME_MIDDLE;
    packet.payload.social.status = is_online ? STATUS_ONLINE : STATUS_OFFLINE;
    packet.payload.social.target_user_id = user_id;
    strncpy(packet.payload.social.target_username, username, MAX_NAME_LEN - 1);
    packet.payload.social.target_username[MAX_NAME_LEN - 1] = '\0';
    strncpy(packet.payload.social.target_status, status_text ? status_text : "", PROFILE_STATUS_LEN);
    packet.payload.social.target_status[PROFILE_STATUS_LEN] = '\0';
    packet.payload.social.status_code = is_online;

    write(ctx->client_fd, &packet, sizeof(TizcordPacket));
}

static int client_is_online(ServerContext *ctx, int64_t user_id) {
    if (!ctx) return 0;

    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].socket_fd > 0 &&
            ctx->clients[i].id == user_id &&
            ctx->clients[i].is_authenticated) {
            return 1;
        }
    }

    return 0;
}

static void send_status_update_packet(int client_fd, int status_code, int64_t user_id,
                                      const char *username, const char *status_text,
                                      int is_online) {
    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_UPDATE_STATUS;
    packet.payload.social.status_code = status_code;
    packet.payload.social.status = is_online ? STATUS_ONLINE : STATUS_OFFLINE;
    packet.payload.social.target_user_id = user_id;

    if (username != NULL) {
        strncpy(packet.payload.social.target_username, username, MAX_NAME_LEN - 1);
        packet.payload.social.target_username[MAX_NAME_LEN - 1] = '\0';
    }

    if (status_text != NULL) {
        strncpy(packet.payload.social.target_status, status_text, PROFILE_STATUS_LEN);
        packet.payload.social.target_status[PROFILE_STATUS_LEN] = '\0';
    }

    write(client_fd, &packet, sizeof(TizcordPacket));
}

static void notify_user_status_update(ServerContext *ctx, int64_t user_id,
                                      const char *username, const char *status_text) {
    int is_online = client_is_online(ctx, user_id);

    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].socket_fd > 0 && ctx->clients[i].is_authenticated) {
            send_status_update_packet(ctx->clients[i].socket_fd, RESP_OK, user_id,
                                      username, status_text, is_online);
        }
    }
}

// Send a refreshed friend list to a user if they are currently online
static void notify_friend_list_update(ServerContext *ctx, int64_t target_user_id) {
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].socket_fd > 0 && ctx->clients[i].id == target_user_id) {
            int fd = ctx->clients[i].socket_fd;

            FriendRequestListContext cb_ctx = {
                .client_fd = fd,
                .current_index = 0,
                .list_id = (int32_t)target_user_id
            };

            TizcordPacket start_pkt = create_base_packet(PACKET_SOCIAL);
            start_pkt.payload.social.action = SOCIAL_LIST_FRIENDS;
            start_pkt.list_id = cb_ctx.list_id;
            start_pkt.list_frame = LIST_FRAME_START;
            write(fd, &start_pkt, sizeof(TizcordPacket));

            db_list_friend_requests(ctx->db, target_user_id, friend_request_cb, &cb_ctx);

            TizcordPacket end_pkt = create_base_packet(PACKET_SOCIAL);
            end_pkt.payload.social.action = SOCIAL_LIST_FRIENDS;
            end_pkt.list_id = cb_ctx.list_id;
            end_pkt.list_total = cb_ctx.current_index;
            end_pkt.list_frame = LIST_FRAME_END;
            write(fd, &end_pkt, sizeof(TizcordPacket));
            break;
        }
    }
}

void notify_all_user_lists(ServerContext *ctx) {
    if (ctx == NULL || ctx->db == NULL) {
        return;
    }

    for (int i = 0; i < ctx->client_count; i++) {
        ClientNode *client = &ctx->clients[i];
        if (client->socket_fd <= 0 || !client->is_authenticated) {
            continue;
        }

        UserListContext ulc = {
            .srv_ctx = ctx,
            .client_fd = client->socket_fd,
            .current_index = 0,
            .list_id = (int32_t)client->id
        };

        TizcordPacket start_pkt = create_base_packet(PACKET_SOCIAL);
        start_pkt.payload.social.action = SOCIAL_LIST_USERS;
        start_pkt.list_id = ulc.list_id;
        start_pkt.list_frame = LIST_FRAME_START;
        write(client->socket_fd, &start_pkt, sizeof(TizcordPacket));

        db_list_all_users(ctx->db, user_list_cb, &ulc);

        TizcordPacket end_pkt = create_base_packet(PACKET_SOCIAL);
        end_pkt.payload.social.action = SOCIAL_LIST_USERS;
        end_pkt.list_id = ulc.list_id;
        end_pkt.list_total = ulc.current_index;
        end_pkt.list_frame = LIST_FRAME_END;
        write(client->socket_fd, &end_pkt, sizeof(TizcordPacket));
    }
}

void handle_social_packet(ServerContext *ctx, ClientNode *client, TizcordPacket *packet) {
    if (!ctx || !client || !packet) return;

    if (!client->is_authenticated) {
        send_action_response(client->socket_fd, PACKET_SOCIAL, packet->payload.social.action, RESP_ERR_UNAUTHORIZED, NULL);
        return;
    }

    switch (packet->payload.social.action) {
        case SOCIAL_FRIEND_REQUEST: {
            int64_t friend_id = 0;
            if (db_get_user_id_by_name(ctx->db, packet->payload.social.target_username, &friend_id) != 0) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REQUEST, RESP_ERR_NOT_FOUND, "User not found.");
                return;
            }
            if (friend_id == client->id) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REQUEST, RESP_ERR_INVALID, "You cannot send a friend request to yourself.");
                return;
            }

            int rc = db_send_friend_request(ctx->db, client->id, friend_id);
            if (rc == -2) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REQUEST, RESP_ERR_CONFLICT, "You are already friends!");
                return;
            }
            if (rc < 0) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REQUEST, RESP_ERR_DB, "Friend request failed.");
                return;
            }

            if (rc == 1) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REQUEST, RESP_OK, "Incoming request found. Friendship accepted!");
                notify_friend_list_update(ctx, client->id);
                notify_friend_list_update(ctx, friend_id);
                return;
            }

            send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REQUEST, RESP_OK, "Friend request sent!");
            notify_friend_list_update(ctx, friend_id);
            break;
        }
        case SOCIAL_FRIEND_ACCEPT: {
            int64_t friend_id = 0;
            if (db_get_user_id_by_name(ctx->db, packet->payload.social.target_username, &friend_id) != 0) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_ACCEPT, RESP_ERR_NOT_FOUND, "User not found.");
                return;
            }

            int rc = db_accept_friend_request(ctx->db, client->id, friend_id);
            if (rc == -1) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_ACCEPT, RESP_ERR_NOT_FOUND, "No incoming request from this user.");
                return;
            } else if (rc == -2) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_ACCEPT, RESP_ERR_CONFLICT, "You are already friends!");
                return;
            }

            send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_ACCEPT, RESP_OK, "Friend request accepted!");
            notify_friend_list_update(ctx, friend_id);
            break;
        }
        case SOCIAL_LIST_FRIENDS: {
            FriendRequestListContext cb_ctx = {
                .client_fd = client->socket_fd,
                .current_index = 0,
                .list_id = (int32_t)client->id
            };

            TizcordPacket start_pkt = create_base_packet(PACKET_SOCIAL);
            start_pkt.payload.social.action = SOCIAL_LIST_FRIENDS;
            start_pkt.list_id = cb_ctx.list_id;
            start_pkt.list_frame = LIST_FRAME_START;
            write(client->socket_fd, &start_pkt, sizeof(TizcordPacket));

            db_list_friend_requests(ctx->db, client->id, friend_request_cb, &cb_ctx);

            TizcordPacket end_pkt = create_base_packet(PACKET_SOCIAL);
            end_pkt.payload.social.action = SOCIAL_LIST_FRIENDS;
            end_pkt.list_id = cb_ctx.list_id;
            end_pkt.list_total = cb_ctx.current_index;
            end_pkt.list_frame = LIST_FRAME_END;
            write(client->socket_fd, &end_pkt, sizeof(TizcordPacket));
            break;
        }
        case SOCIAL_FRIEND_REMOVE: {
            int64_t friend_id = 0;
            if (db_get_user_id_by_name(ctx->db, packet->payload.social.target_username, &friend_id) != 0) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REMOVE, RESP_ERR_NOT_FOUND, "User not found.");
                return;
            }

            int rc = db_remove_friendship(ctx->db, client->id, friend_id);
            if (rc == -1) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REMOVE, RESP_ERR_NOT_FOUND, "You are not friends with this user.");
                return;
            }

            send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REMOVE, RESP_OK, "Friend removed.");
            notify_friend_list_update(ctx, friend_id);
            break;
        }
        case SOCIAL_FRIEND_REJECT: {
            int64_t friend_id = 0;
            if (db_get_user_id_by_name(ctx->db, packet->payload.social.target_username, &friend_id) != 0) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REJECT, RESP_ERR_NOT_FOUND, "User not found.");
                return;
            }

            int rc = db_reject_friend_request(ctx->db, client->id, friend_id);
            if (rc == -1) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REJECT, RESP_ERR_NOT_FOUND, "No incoming request from this user.");
                return;
            }

            send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REJECT, RESP_OK, "Friend request rejected.");
            notify_friend_list_update(ctx, friend_id);
            break;
        }
        case SOCIAL_LIST_USERS: {
            UserListContext ulc = {
                .srv_ctx = ctx,
                .client_fd = client->socket_fd,
                .current_index = 0,
                .list_id = (int32_t)client->id
            };

            TizcordPacket start_pkt = create_base_packet(PACKET_SOCIAL);
            start_pkt.payload.social.action = SOCIAL_LIST_USERS;
            start_pkt.list_id = ulc.list_id;
            start_pkt.list_frame = LIST_FRAME_START;
            write(client->socket_fd, &start_pkt, sizeof(TizcordPacket));

            db_list_all_users(ctx->db, user_list_cb, &ulc);

            TizcordPacket end_pkt = create_base_packet(PACKET_SOCIAL);
            end_pkt.payload.social.action = SOCIAL_LIST_USERS;
            end_pkt.list_id = ulc.list_id;
            end_pkt.list_total = ulc.current_index;
            end_pkt.list_frame = LIST_FRAME_END;
            write(client->socket_fd, &end_pkt, sizeof(TizcordPacket));
            break;
        }
        case SOCIAL_UPDATE_STATUS: {
            size_t status_len = strnlen(packet->payload.social.target_status, PROFILE_STATUS_LEN + 1);

            if (status_len == 0 || status_len > PROFILE_STATUS_LEN) {
                send_status_update_packet(client->socket_fd, RESP_ERR_INVALID, client->id,
                                          client->username, NULL, 1);
                return;
            }

            if (db_update_user_status(ctx->db, client->id, packet->payload.social.target_status) != 0) {
                send_status_update_packet(client->socket_fd, RESP_ERR_DB, client->id,
                                          client->username, NULL, 1);
                return;
            }

            notify_user_status_update(ctx, client->id, client->username,
                                      packet->payload.social.target_status);
            break;
        }
        default:
            send_action_response(client->socket_fd, PACKET_SOCIAL, packet->payload.social.action, RESP_ERR_INVALID, "Invalid action.");
            break;
    }
}
