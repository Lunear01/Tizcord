#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include "protocol.h"  
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

int send_register(const char *username, const char *password) {
    if (client_socket < 0) return -1;

    TizcordPacket packet = create_base_packet(MSG_LOGIN);
    packet.payload.auth.action = AUTH_REGISTER;
    strncpy(packet.payload.auth.username, username, sizeof(packet.payload.auth.username) - 1);
    strncpy(packet.payload.auth.password, password, sizeof(packet.payload.auth.password) - 1);

    write(client_socket, &packet, sizeof(TizcordPacket));
    
    return -1; // Network failure
}

int send_login(const char *username, const char *password) {
    if (client_socket < 0) return -1;

    TizcordPacket packet = create_base_packet(MSG_LOGIN);
    packet.payload.auth.action = AUTH_LOGIN;
    strncpy(packet.payload.auth.username, username, sizeof(packet.payload.auth.username) - 1);
    strncpy(packet.payload.auth.password, password, sizeof(packet.payload.auth.password) - 1);

    write(client_socket, &packet, sizeof(TizcordPacket));
    
    return 0; 
}

void create_server(const char *server_name) {
    if (client_socket < 0) return;
    
    TizcordPacket packet = create_base_packet(MSG_SERVER);
    
    packet.payload.server.action = SERVER_CREATE;
    strncpy(packet.payload.server.server_name, server_name, sizeof(packet.payload.server.server_name) - 1);

    printf("[Client] Sending request to create server: %s\n", server_name);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void leave_server(int server_id) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_SERVER);
    
    packet.payload.server.action = SERVER_LEAVE;
    snprintf((char *)packet.payload.server.server_id, sizeof(packet.payload.server.server_id), "%d", server_id);

    printf("[Client] Sending request to leave server ID: %d\n", server_id);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void create_channel(int server_id, const char *channel_name) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_CHANNEL);
    
    // Assuming ChannelPayload has fields for this
    // packet.payload.channel.server_id = server_id;
    // strncpy(packet.payload.channel.channel_name, channel_name, 31);
    packet.payload.channel.action = CHANNEL_CREATE;
    snprintf((char *)packet.payload.channel.server_id, sizeof(packet.payload.channel.server_id), "%d", server_id);
    strncpy(packet.payload.channel.channel_name, channel_name, sizeof(packet.payload.channel.channel_name) - 1);

    printf("[Client] Sending request to create channel '%s' in server %d\n", channel_name, server_id);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void delete_channel(int channel_id) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_CHANNEL);
    
    packet.payload.channel.action = CHANNEL_DELETE;
    snprintf((char *)packet.payload.channel.channel_id, sizeof(packet.payload.channel.channel_id), "%d", channel_id);

    printf("[Client] Sending request to delete channel ID: %d\n", channel_id);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void send_friend_request(const char *target_username) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_DM);
    
    packet.payload.dm.action = DM_FRIEND_REQUEST;
    strncpy(packet.receiver, target_username, sizeof(packet.receiver) - 1);

    printf("[Client] Sending friend request to %s...\n", target_username);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void accept_friend_request(const char *target_username) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(MSG_DM);
    
    strncpy(packet.receiver, target_username, sizeof(packet.receiver) - 1);
    
    packet.payload.dm.action = DM_FRIEND_ACCEPT;
    strncpy(packet.receiver, target_username, sizeof(packet.receiver) - 1);

    printf("[Client] Accepting friend request from %s...\n", target_username);
    write(client_socket, &packet, sizeof(TizcordPacket));
}
