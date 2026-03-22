#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include "protocol.h" 
#include "server.h"   
#include "client.h"   

// Global socket for the client connection
int client_socket = -1;

// Helper function to initialize a base packet
TizcordPacket create_base_packet(PacketType type) {
    TizcordPacket packet;
    memset(&packet, 0, sizeof(TizcordPacket));
    packet.type = type;
    packet.timestamp = time(NULL);
    // In a real app, you'd set the logged-in user's name here:
    strcpy(packet.sender, "CurrentUser");
    return packet;
}

void connect_to_server(const char *ip_address, int port) {
    struct sockaddr_in serv_addr;

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("\nSocket creation error\n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); 

    // Convert IPv4 addresses from text to binary form
    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address or Address not supported\n");
        return;
    }

    if (connect(client_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed\n");
        return;
    }
    
    printf("[Client] Successfully connected to %s:%d!\n", ip_address, port);
}

void create_server(const char *server_name) {
    if (client_socket < 0) return;
    
    TizcordPacket packet = create_base_packet(MSG_SERVER);
    
    // Assuming ServerPayload has a server_name field
    // strncpy(packet.payload.server.server_name, server_name, 31);

    printf("[Client] Sending request to create server: %s\n", server_name);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void leave_server(int server_id) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_SERVER);
    
    // Assuming ServerPayload has a server_id field
    // packet.payload.server.server_id = server_id;
    
    printf("[Client] Sending request to leave server ID: %d\n", server_id);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void create_channel(int server_id, const char *channel_name) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_CHANNEL);
    
    // Assuming ChannelPayload has fields for this
    // packet.payload.channel.server_id = server_id;
    // strncpy(packet.payload.channel.channel_name, channel_name, 31);

    printf("[Client] Sending request to create channel '%s' in server %d\n", channel_name, server_id);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void delete_channel(int channel_id) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_CHANNEL);
    
    // Assuming ChannelPayload has a channel_id field
    // packet.payload.channel.channel_id = channel_id;

    printf("[Client] Sending request to delete channel ID: %d\n", channel_id);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void send_friend_request(const char *target_username) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_DM);
    
    // Use the receiver field from TizcordPacket for routing
    strncpy(packet.receiver, target_username, sizeof(packet.receiver) - 1);

    printf("[Client] Sending friend request to %s...\n", target_username);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void accept_friend_request(const char *target_username) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_DM);
    
    strncpy(packet.receiver, target_username, sizeof(packet.receiver) - 1);
    
    // In a real implementation, you might need a flag in DMPayload 
    // to differentiate an "accept" from a standard "request" or "message"

    printf("[Client] Accepting friend request from %s...\n", target_username);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void update_user_status(ServerContext *ctx, int client_index, UserStatus new_status) {
    // This utilizes the ServerContext which tracks the array of connected clients
    if (ctx != NULL && client_index >= 0 && client_index < ctx->client_count) {
        printf("[Client/Server] Updating status for user %s\n", ctx->clients[client_index].username);
    }
}