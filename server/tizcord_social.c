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

            if (db_send_friend_request(ctx->db, client->id, friend_id) != 0) {
                send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REQUEST, RESP_ERR_DB, "Friend request failed.");
                return;
            }

            send_action_response(client->socket_fd, PACKET_SOCIAL, SOCIAL_FRIEND_REQUEST, RESP_OK, "Friend request sent!");
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
        default:
            send_action_response(client->socket_fd, PACKET_SOCIAL, packet->payload.social.action, RESP_ERR_INVALID, "Invalid action.");
            break;
    }
}
