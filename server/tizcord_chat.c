/* chat methods */
#include "../shared/protocol.h"
#include "../shared/packet_helper.h"
#include "../include/server.h"
#include "../include/db.h"
#include "../include/tizcord_channel.h"
#include "../include/tizcord_chat.h"
#include "../include/tizcord_server.h"

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
    
    write(ctx->client_fd, &packet, sizeof(TizcordPacket));
}

void channel_broadcast(ServerContext *ctx, sqlite3_int64 channel_id, const char *packet, size_t packet_size, int sender_fd) {
    (void)channel_id; // Will be used later to filter by specific channels
    
    for (int i = 0; i < ctx->client_count; i++) {
        // Only send to active sockets, and don't echo back to the sender
        if (ctx->clients[i].socket_fd > 0 && ctx->clients[i].socket_fd != sender_fd) {
            write(ctx->clients[i].socket_fd, packet, packet_size);
        }
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
            // We use channel_name to safely transmit the sender's username to the UI
            strncpy(packet->payload.channel.channel_name, sender_node->username, MAX_NAME_LEN - 1);
        }
        
        // Broadcast the modified packet
        channel_broadcast(ctx, 
                          packet->payload.channel.channel_id,
                          (const char*)packet, 
                          sizeof(TizcordPacket), 
                          -1); // Changed to -1 so the sender also receives the broadcast!
        
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
        
        // ADMIN CHECK
        if (sender_node != NULL && ctx->db != NULL &&
            db_user_is_server_admin(ctx->db, packet->payload.channel.server_id, sender_node->id, &is_admin) == 0 && is_admin) {
            
            int64_t new_channel_id = 0;
            if (db_create_channel(ctx->db, packet->payload.channel.server_id, packet->payload.channel.channel_name, &new_channel_id) == 0) {
                reply.payload.channel.status_code = 0; 
                reply.payload.channel.channel_id = new_channel_id; 
                strncpy(reply.payload.channel.channel_name, packet->payload.channel.channel_name, MAX_NAME_LEN - 1);
            } else {
                reply.payload.channel.status_code = -5; 
            }
        } else {
            reply.payload.channel.status_code = 401; // Unauthorized
        }
        write(sender_fd, &reply, sizeof(TizcordPacket));
<<<<<<< HEAD
=======
    } else if (packet->payload.channel.action == CHANNEL_DELETE) {
        int is_admin = 0;
        int64_t server_id = 0;
        
        TizcordPacket reply = create_base_packet(PACKET_CHANNEL);
        reply.payload.channel.action = CHANNEL_DELETE;
        reply.payload.channel.channel_id = packet->payload.channel.channel_id;
        
        if (sender_node != NULL && ctx->db != NULL) {
            
            // Get the server ID this channel belongs to
            if (db_get_channel_server_id(ctx->db, packet->payload.channel.channel_id, &server_id) == 0) {
                
                // ADMIN CHECK
                if (db_user_is_server_admin(ctx->db, server_id, sender_node->id, &is_admin) == 0 && is_admin) {
                    
                    if (db_delete_channel(ctx->db, packet->payload.channel.channel_id) == 0) {
                        reply.payload.channel.status_code = 0;
                    } else {
                        reply.payload.channel.status_code = -5;
                    }
                } else {
                    reply.payload.channel.status_code = 401; // Unauthorized
                }
            } else {
                reply.payload.channel.status_code = RESP_ERR_NOT_FOUND;
            }
        }
        write(sender_fd, &reply, sizeof(TizcordPacket));
    } else if (packet->payload.channel.action == CHANNEL_MESSAGE_EDIT) {
        printf("[Chat] Edit channel message requested for ID: %lld\n", (long long)packet->payload.channel.message_id);
        
        if (ctx->db != NULL) {
            db_edit_message(ctx->db, packet->payload.channel.message_id, packet->payload.channel.message);
        }

        // Broadcast the edit packet to everyone in the channel
        channel_broadcast(ctx, 
                          packet->payload.channel.channel_id, 
                          (const char*)packet, 
                          sizeof(TizcordPacket), 
                          sender_fd);
    } 
    else if (packet->payload.channel.action == CHANNEL_MESSAGE_DELETE) {
        int is_admin = 0;
        int64_t target_channel_id = 0;
        
        TizcordPacket reply = create_base_packet(PACKET_CHANNEL);
        reply.payload.channel.action = CHANNEL_DELETE;
        
        // Ensure the database is available and the sender is a known active client
        if (sender_node != NULL && ctx->db != NULL) {
            
            // Verify the user is an admin of the server they are trying to modify
            if (db_user_is_server_admin(ctx->db, packet->payload.channel.server_id, sender_node->id, &is_admin) == 0 && is_admin) {
                
                // Fetch the channel ID using your new function
                // (Assuming your client sends the channel_name and server_id to be deleted)
                if (db_get_channel_id(ctx->db, packet->payload.channel.server_id, packet->payload.channel.channel_name, &target_channel_id) == 0) {
                    
                    // Delete the channel
                    if (db_delete_channel(ctx->db, target_channel_id) == 0) {
                        reply.payload.channel.status_code = 0; // Success
                    } else {
                        reply.payload.channel.status_code = -5; // DB Error
                    }
                } else {
                    reply.payload.channel.status_code = RESP_ERR_NOT_FOUND; // Channel doesn't exist
                }
            } else {
                reply.payload.channel.status_code = RESP_ERR_UNAUTHORIZED; // Not an admin
            }
        } else {
             reply.payload.channel.status_code = RESP_ERR_INTERNAL;
        }
        
        write(sender_fd, &reply, sizeof(TizcordPacket));
>>>>>>> 5a8cd7ac945ae4f6b052572c8c36814d63035465
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
        write(sender_fd, &start_pkt, sizeof(TizcordPacket));

        // Stream the history
        if (ctx->db != NULL) {
            db_list_channel_messages(ctx->db, packet->payload.channel.channel_id, message_history_cb, &cb_ctx);
        }

        // Send END frame
        TizcordPacket end_pkt = create_base_packet(PACKET_CHANNEL);
        end_pkt.payload.channel.action = CHANNEL_MESSAGE;
        end_pkt.list_id = cb_ctx.list_id;
        end_pkt.list_frame = LIST_FRAME_END;
        write(sender_fd, &end_pkt, sizeof(TizcordPacket));
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

void handle_private_message(ServerContext *ctx, TizcordPacket *packet, int sender_fd) {
    if (packet->payload.dm.action == DM_MESSAGE) {
        
        // 1. Find the sender to securely get their ID
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

        printf("[Chat] Routing PACKET_DM from %lld to %lld\n", 
               (long long)packet->sender_id, (long long)packet->payload.dm.recipient_id);
        
        int found = 0;
        // 2. Find recipient by matching ID in the active client array
        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].socket_fd > 0 && ctx->clients[i].id == packet->payload.dm.recipient_id) {
                
                // Forward the exact packet to the receiver's socket
                write(ctx->clients[i].socket_fd, packet, sizeof(TizcordPacket));
                printf("[Chat] PACKET_DM delivered to ID %lld\n", (long long)packet->payload.dm.recipient_id);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            printf("[Chat] Message failed: User %lld not found or offline.\n", (long long)packet->payload.dm.recipient_id);
            
            // Route an error packet back to sender_fd
            TizcordPacket error_reply;
            memcpy(&error_reply, packet, sizeof(TizcordPacket)); // Copy original details
            error_reply.payload.dm.status_code = 404; // Standard not found code
            strcpy(error_reply.payload.dm.message, "Error: User is offline or does not exist.");
            
            write(sender_fd, &error_reply, sizeof(TizcordPacket));
        }
    }
}
