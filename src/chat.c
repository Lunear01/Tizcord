#include "../include/chat.h"
#include "../include/server.h"
#include "../include/protocol.h"
#include "../include/db.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

void handle_chat_packet(ServerContext *ctx, TizcordPacket *packet, int sender_fd) {
    if (!ctx || !packet) return;

    if (packet->type == MSG_DM) {
        handle_private_message(ctx, packet, sender_fd);
    } else if (packet->type == MSG_CHANNEL) {
        handle_channel_message(ctx, packet, sender_fd);
    }
}

void handle_private_message(ServerContext *ctx, TizcordPacket *packet, int sender_fd) {
    if (packet->payload.dm.action == DM_MESSAGE) {
        printf("[Chat] Routing DM from %s to %s\n", packet->sender, packet->receiver);
        
        int found = 0;
        // Find recipient by matching username in the active client array
        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].socket_fd > 0 && strcmp(ctx->clients[i].username, packet->receiver) == 0) {
                
                // Forward the exact packet to the receiver's socket
                write(ctx->clients[i].socket_fd, packet, sizeof(TizcordPacket));
                printf("[Chat] DM delivered to %s\n", packet->receiver);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            printf("[Chat] Message failed: User %s not found or offline.\n", packet->receiver);
            
            // Route an error packet back to sender_fd
            TizcordPacket error_reply;
            memcpy(&error_reply, packet, sizeof(TizcordPacket)); // Copy original details
            error_reply.payload.dm.status_code = 404; // Standard not found code
            strcpy(error_reply.payload.dm.message, "Error: User is offline or does not exist.");
            
            write(sender_fd, &error_reply, sizeof(TizcordPacket));
        }
    } 
    else if (packet->payload.dm.action == DM_MESSAGE_EDIT) {
        printf("[Chat] Edit DM requested for message ID: %s\n", packet->payload.dm.message_id);
        
        if (ctx->db != NULL) {
            db_edit_message(ctx->db, packet->payload.dm.message_id, packet->payload.dm.message);
        }

        // Forward edit packet to the receiver so their UI updates
        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].socket_fd > 0 && strcmp(ctx->clients[i].username, packet->receiver) == 0) {
                write(ctx->clients[i].socket_fd, packet, sizeof(TizcordPacket));
                break;
            }
        }
    } 
    else if (packet->payload.dm.action == DM_MESSAGE_DELETE) {
        printf("[Chat] Delete DM requested for message ID: %s\n", packet->payload.dm.message_id);
        
        if (ctx->db != NULL) {
            db_delete_message(ctx->db, packet->payload.dm.message_id);
        }

        // Forward delete packet to the receiver so their UI updates
        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].socket_fd > 0 && strcmp(ctx->clients[i].username, packet->receiver) == 0) {
                write(ctx->clients[i].socket_fd, packet, sizeof(TizcordPacket));
                break;
            }
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
        // Updated printf to use %s for the UUID string
        printf("[Chat] Broadcast request from %s to Channel ID: %s\n", 
               packet->sender, packet->payload.channel.channel_id);
        
        // Broadcast the entire packet
        channel_broadcast(ctx, 
                          0, // Temporary placeholder index
                          (const char*)packet, 
                          sizeof(TizcordPacket), 
                          sender_fd);
        
        if (ctx->db != NULL && sender_node != NULL) {
            db_save_message(ctx->db, 
                            packet->payload.channel.channel_id, 
                            sender_node->id, 
                            packet->payload.channel.message);
        }
    }
    else if (packet->payload.channel.action == CHANNEL_MESSAGE_EDIT) {
        printf("[Chat] Edit channel message requested for ID: %s\n", packet->payload.channel.message_id);
        
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
        printf("[Chat] Delete channel message requested for ID: %s\n", packet->payload.channel.message_id);
        
        if (ctx->db != NULL) {
            db_delete_message(ctx->db, packet->payload.channel.message_id);
        }

        // Broadcast the delete packet to everyone in the channel
        channel_broadcast(ctx, 
                          packet->payload.channel.channel_id, 
                          (const char*)packet, 
                          sizeof(TizcordPacket), 
                          sender_fd);
    }
}