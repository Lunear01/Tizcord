/* chat methods */
#include "../shared/protocol.h"
#include "../shared/packet_helper.h"
#include "include/server.h"
#include "include/db.h"
#include "include/tizcord_chat.h"
#include "include/client_helper.h"
#include "include/tizcord_server.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>

typedef struct {
    int client_fd;
    int32_t current_index;
    int32_t list_id;
    int64_t channel_id;
} MessageHistoryContext;

typedef struct {
    int client_fd;
    int32_t current_index;
    int32_t list_id;
    int64_t my_user_id;
    int64_t friend_id;
    char my_username[MAX_NAME_LEN];
} DMHistoryContext;

static void message_history_cb(int64_t msg_id, const char* username, const char* content, int64_t timestamp, void* userdata) {
    MessageHistoryContext *ctx = (MessageHistoryContext *)userdata;
    TizcordPacket packet = create_base_packet(PACKET_CHANNEL);
    
    packet.payload.channel.action = CHANNEL_MESSAGE;
    packet.list_id = ctx->list_id;
    packet.list_index = ctx->current_index++;
    packet.list_frame = LIST_FRAME_MIDDLE;
    
    packet.payload.channel.channel_id = ctx->channel_id;
    packet.payload.channel.message_id = msg_id;
    packet.timestamp = timestamp;

    strncpy(packet.payload.channel.channel_name, username, MAX_NAME_LEN - 1);
    strncpy(packet.payload.channel.message, content, MESSAGE_LEN - 1);
    
    send_packet_to_client(ctx->client_fd, &packet);
}

static void direct_message_history_cb(int64_t msg_id, const char* sender_username, const char* receiver_username, const char* content, int64_t timestamp, void* userdata) {
    (void)receiver_username; 

    DMHistoryContext *ctx = (DMHistoryContext *)userdata;
    TizcordPacket packet = create_base_packet(PACKET_DM);
    
    packet.payload.dm.action = DM_MESSAGE;
    packet.list_id = ctx->list_id;
    packet.list_index = ctx->current_index++;
    packet.list_frame = LIST_FRAME_MIDDLE;
    
    // Determine the sender ID by comparing the username from the DB 
    // against the user requesting the history.
    if (strcmp(sender_username, ctx->my_username) == 0) {
        packet.sender_id = ctx->my_user_id; // I sent this message
    } else {
        packet.sender_id = ctx->friend_id;  // My friend sent this message
    }
    
    packet.payload.dm.recipient_id = ctx->friend_id; // Always the context of the conversation
    packet.payload.dm.message_id = msg_id;
    packet.timestamp = timestamp;
    
    strncpy(packet.payload.dm.message, content, MESSAGE_LEN - 1);
    
    send_packet_to_client(ctx->client_fd, &packet);
}

void channel_broadcast(ServerContext *ctx, sqlite3_int64 channel_id, const char *packet, size_t packet_size, int sender_fd) {
    (void)channel_id; // Will be used later to filter by specific channels
    
    for (int i = 0; i < ctx->client_count; i++) {
        // Only send to active sockets, and don't echo back to the sender
        if (ctx->clients[i].socket_fd > 0 && ctx->clients[i].socket_fd != sender_fd) {
            if (packet_size == sizeof(TizcordPacket)) {
                send_packet_to_client(ctx->clients[i].socket_fd,
                                      (const TizcordPacket *)packet);
            }
        }
    }
}

void handle_chat_packet(ServerContext *ctx, TizcordPacket *packet, int sender_fd) {
    if (!ctx || !packet) return;

    if (packet->type == PACKET_DM) {
        handle_private_message(ctx, packet, sender_fd);
    } else if (packet->type == PACKET_CHANNEL) {
        handle_channel_message(ctx, packet, sender_fd);
    }
}

void handle_channel_message(ServerContext *ctx, TizcordPacket *packet, int sender_fd) {
    // Helper to find the actual ClientNode of the sender to get their UUID
    ClientNode *sender_node = NULL;
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].socket_fd == sender_fd) {
            sender_node = &ctx->clients[i];
            break;
        }
    }

    if (packet->payload.channel.action == CHANNEL_MESSAGE) {
        printf("[Chat] Broadcast request from %lld to Channel ID: %lld\n", 
               (long long)packet->sender_id, (long long)packet->payload.channel.channel_id);
        
        // Inject the authoritative sender info before broadcasting
        if (sender_node != NULL) {
            packet->sender_id = sender_node->id;
            // Use channel_name to safely transmit the sender's username to the UI
            strncpy(packet->payload.channel.channel_name, sender_node->username, MAX_NAME_LEN - 1);
        }
        
        // Broadcast the modified packet
        channel_broadcast(ctx, 
                          packet->payload.channel.channel_id,
                          (const char*)packet, 
                          sizeof(TizcordPacket), 
                          -1); // The sender also receives the broadcast
        
        if (ctx->db != NULL && sender_node != NULL) {
            db_save_message(ctx->db, 
                            packet->payload.channel.channel_id, 
                            (sqlite3_int64)sender_node->id, 
                            packet->payload.channel.message);
        }
    }
    else if (packet->payload.channel.action == CHANNEL_CREATE) {
        printf("[Chat] Create channel request: %s in Server ID: %lld\n", 
               packet->payload.channel.channel_name, (long long)packet->payload.channel.server_id);
        
        TizcordPacket reply = create_base_packet(PACKET_CHANNEL);
        reply.payload.channel.action = CHANNEL_CREATE;
        reply.payload.channel.server_id = packet->payload.channel.server_id;
        
        int is_admin = 0;
        
        // Admin check
        if (sender_node != NULL && ctx->db != NULL &&
            db_user_is_server_admin(ctx->db, packet->payload.channel.server_id, sender_node->id, &is_admin) == 0 && is_admin) {
            
            int64_t new_channel_id = 0;
            if (db_create_channel(ctx->db, packet->payload.channel.server_id, packet->payload.channel.channel_name, &new_channel_id) == 0) {
                reply.payload.channel.status_code = RESP_OK;
                reply.payload.channel.channel_id = new_channel_id; 
                strncpy(reply.payload.channel.channel_name, packet->payload.channel.channel_name, MAX_NAME_LEN - 1);
            } else {
                reply.payload.channel.status_code = RESP_ERR_DB;
            }
        } else {
            reply.payload.channel.status_code = RESP_ERR_UNAUTHORIZED;
        }
        send_packet_to_client(sender_fd, &reply);

    }

    else if (packet->payload.channel.action == CHANNEL_DELETE) {
        int is_admin = 0;
        int64_t server_id = 0;
        
        TizcordPacket reply = create_base_packet(PACKET_CHANNEL);
        reply.payload.channel.action = CHANNEL_DELETE;
        reply.payload.channel.channel_id = packet->payload.channel.channel_id;
        
        if (sender_node != NULL && ctx->db != NULL) {
            
            // Get the server ID this channel belongs to
            if (db_get_channel_server_id(ctx->db, packet->payload.channel.channel_id, &server_id) == 0) {
                
                // Admine_check
                if (db_user_is_server_admin(ctx->db, server_id, sender_node->id, &is_admin) == 0 && is_admin) {
                    
                    if (db_delete_channel(ctx->db, packet->payload.channel.channel_id) == 0) {
                        reply.payload.channel.status_code = RESP_OK;
                    } else {
                        reply.payload.channel.status_code = RESP_ERR_DB;
                    }
                } else {
                    reply.payload.channel.status_code = RESP_ERR_UNAUTHORIZED;
                }
            } else {
                reply.payload.channel.status_code = RESP_ERR_NOT_FOUND;
            }
        } else {
            reply.payload.channel.status_code = RESP_ERR_INTERNAL;
        }
        send_packet_to_client(sender_fd, &reply);
    }
    else if (packet->payload.channel.action == CHANNEL_HISTORY_REQUEST) {
        printf("[Chat] History request for Channel ID: %lld\n", (long long)packet->payload.channel.channel_id);
        
        MessageHistoryContext cb_ctx = {
            .client_fd = sender_fd,
            .current_index = 0,
            .list_id = (int32_t)packet->payload.channel.channel_id,
            .channel_id = packet->payload.channel.channel_id
        };

        // Send START frame
        TizcordPacket start_pkt = create_base_packet(PACKET_CHANNEL);
        start_pkt.payload.channel.action = CHANNEL_MESSAGE;
        start_pkt.list_id = cb_ctx.list_id;
        start_pkt.list_frame = LIST_FRAME_START;
        send_packet_to_client(sender_fd, &start_pkt);

        // Stream the history
        if (ctx->db != NULL) {
            db_list_channel_messages(ctx->db, packet->payload.channel.channel_id, message_history_cb, &cb_ctx);
        }

        // Send END frame
        TizcordPacket end_pkt = create_base_packet(PACKET_CHANNEL);
        end_pkt.payload.channel.action = CHANNEL_MESSAGE;
        end_pkt.list_id = cb_ctx.list_id;
        end_pkt.list_frame = LIST_FRAME_END;
        send_packet_to_client(sender_fd, &end_pkt);
    }
}

void handle_private_message(ServerContext *ctx, TizcordPacket *packet, int sender_fd) {
    // Find the sender to securely get their ID
    ClientNode *sender_node = NULL;
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].socket_fd == sender_fd) {
            sender_node = &ctx->clients[i];
            break;
        }
    }

    if (sender_node != NULL) {
        // Stamp the authoritative sender ID onto the packet
        packet->sender_id = sender_node->id;
    }

    if (packet->payload.dm.action == DM_MESSAGE) {
        printf("[Chat] Routing PACKET_DM from %lld to %lld\n", 
               (long long)packet->sender_id, (long long)packet->payload.dm.recipient_id);
    
        if (ctx->db != NULL && sender_node != NULL) {
                db_save_direct_message(ctx->db, sender_node->id, packet->payload.dm.recipient_id, packet->payload.dm.message);
        }

        int found = 0;
        // Find recipient by matching ID in the active client array
        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].socket_fd > 0 && ctx->clients[i].id == packet->payload.dm.recipient_id) {
                
                // Forward the exact packet to the receiver's socket
                send_packet_to_client(ctx->clients[i].socket_fd, packet);
                printf("[Chat] PACKET_DM delivered to ID %lld\n", (long long)packet->payload.dm.recipient_id);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            printf("[Chat] Recipient ID %lld not found for PACKET_DM\n", (long long)packet->payload.dm.recipient_id);
        }
    }
        else if (packet->payload.dm.action == DM_HISTORY_REQUEST) {
            if (sender_node == NULL || ctx->db == NULL) return;
            
            DMHistoryContext cb_ctx = {
                .client_fd = sender_fd,
                .current_index = 0,
                .list_id = (int32_t)packet->payload.dm.recipient_id, 
                .my_user_id = sender_node->id,
                .friend_id = packet->payload.dm.recipient_id
            };
            // Copy the requesting user's name into the context
            strncpy(cb_ctx.my_username, sender_node->username, MAX_NAME_LEN - 1);

            TizcordPacket start_pkt = create_base_packet(PACKET_DM);
            start_pkt.payload.dm.action = DM_MESSAGE;
            start_pkt.payload.dm.recipient_id = cb_ctx.friend_id;
            start_pkt.list_id = cb_ctx.list_id;
            start_pkt.list_frame = LIST_FRAME_START;
            send_packet_to_client(sender_fd, &start_pkt);

            // Call the database function
            db_list_direct_messages(ctx->db, sender_node->id, packet->payload.dm.recipient_id, direct_message_history_cb, &cb_ctx);

            TizcordPacket end_pkt = create_base_packet(PACKET_DM);
            end_pkt.payload.dm.action = DM_MESSAGE;
            end_pkt.payload.dm.recipient_id = cb_ctx.friend_id;
            end_pkt.list_id = cb_ctx.list_id;
            end_pkt.list_frame = LIST_FRAME_END;
            send_packet_to_client(sender_fd, &end_pkt);
    }
}
