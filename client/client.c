#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include "protocol.h"  
#include "client.h"   
#include "packet_helper.h"

// Global socket for the client connection
int client_socket = -1;

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

    TizcordPacket packet = create_base_packet(PACKET_AUTH);
    packet.payload.auth.action = AUTH_REGISTER;
    strncpy(packet.payload.auth.username, username, sizeof(packet.payload.auth.username) - 1);
    strncpy(packet.payload.auth.password, password, sizeof(packet.payload.auth.password) - 1);

    write(client_socket, &packet, sizeof(TizcordPacket));
    
    return -1; // Network failure
}

int send_login(const char *username, const char *password) {
    if (client_socket < 0) return -1;

    TizcordPacket packet = create_base_packet(PACKET_AUTH);
    packet.payload.auth.action = AUTH_LOGIN;
    strncpy(packet.payload.auth.username, username, sizeof(packet.payload.auth.username) - 1);
    strncpy(packet.payload.auth.password, password, sizeof(packet.payload.auth.password) - 1);

    write(client_socket, &packet, sizeof(TizcordPacket));
    
    return 0; 
}

void create_server(const char *server_name) {
    if (client_socket < 0) return;
    
    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    
    packet.payload.server.action = SERVER_CREATE;
    strncpy(packet.payload.server.server_name, server_name, sizeof(packet.payload.server.server_name) - 1);

    // printf("[Client] Sending request to create server: %s\n", server_name);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void leave_server(int server_id) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    packet.payload.server.action = SERVER_LEAVE;
    
    // FIX: Assign the integer directly
    packet.payload.server.server_id = server_id; 

    write(client_socket, &packet, sizeof(TizcordPacket));
}

void kick_server_member(int64_t server_id, int64_t target_user_id) {
    if (client_socket < 0) return;
    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    packet.payload.server.action = SERVER_KICK_MEMBER;
    packet.payload.server.server_id = server_id;
    packet.payload.server.target_user_id = target_user_id;
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void create_channel(int server_id, const char *channel_name) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_CHANNEL);
    packet.payload.channel.action = CHANNEL_CREATE;
    
    packet.payload.channel.server_id = server_id; 
    strncpy(packet.payload.channel.channel_name, channel_name, sizeof(packet.payload.channel.channel_name) - 1);

    write(client_socket, &packet, sizeof(TizcordPacket));
}

void delete_server(int64_t server_id) {
    if (client_socket < 0) return;
    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    packet.payload.server.action = SERVER_DELETE;
    packet.payload.server.server_id = server_id;
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void delete_channel(int64_t channel_id) { 
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_CHANNEL);
    packet.payload.channel.action = CHANNEL_DELETE;
    packet.payload.channel.channel_id = channel_id;

    write(client_socket, &packet, sizeof(TizcordPacket));
}

void send_friend_request(const char *target_username) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_FRIEND_REQUEST;
    strncpy(packet.payload.social.target_username, target_username, sizeof(packet.payload.social.target_username) - 1);
    packet.payload.social.target_username[sizeof(packet.payload.social.target_username) - 1] = '\0';

    // printf("[Client] Sending friend request to %s...\n", target_username);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void accept_friend_request(const char *target_username) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_FRIEND_ACCEPT;
    strncpy(packet.payload.social.target_username, target_username, sizeof(packet.payload.social.target_username) - 1);
    packet.payload.social.target_username[sizeof(packet.payload.social.target_username) - 1] = '\0';

    // printf("[Client] Accepting friend request from %s...\n", target_username);
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void unfriend(const char *target_username) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_FRIEND_REMOVE;
    strncpy(packet.payload.social.target_username, target_username, sizeof(packet.payload.social.target_username) - 1);
    packet.payload.social.target_username[sizeof(packet.payload.social.target_username) - 1] = '\0';

    write(client_socket, &packet, sizeof(TizcordPacket));
}

void reject_friend_request(const char *target_username) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_FRIEND_REJECT;
    strncpy(packet.payload.social.target_username, target_username, sizeof(packet.payload.social.target_username) - 1);
    packet.payload.social.target_username[sizeof(packet.payload.social.target_username) - 1] = '\0';

    write(client_socket, &packet, sizeof(TizcordPacket));
}

void send_status_update(const char *status_text) {
    if (client_socket < 0 || status_text == NULL) return;

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_UPDATE_STATUS;
    strncpy(packet.payload.social.target_status, status_text, PROFILE_STATUS_LEN);
    packet.payload.social.target_status[PROFILE_STATUS_LEN] = '\0';

    write(client_socket, &packet, sizeof(TizcordPacket));
}

void list_joined_servers_request(void) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    packet.payload.server.action = SERVER_LIST_JOINED;

    // printf("[Client] Requesting joined server list...\n");
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void request_friend_list(void) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_LIST_FRIENDS;

    // printf("[Client] Refreshing friend list...\n");
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void request_user_list(void) {
    if (client_socket < 0) return;

    TizcordPacket packet = create_base_packet(PACKET_SOCIAL);
    packet.payload.social.action = SOCIAL_LIST_USERS;

    write(client_socket, &packet, sizeof(TizcordPacket));
}

void request_server_channels(int64_t server_id) {
    if (client_socket < 0) return;
    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    packet.payload.server.action = SERVER_LIST_CHANNELS;
    packet.payload.server.server_id = server_id;
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void request_server_members(int64_t server_id) {
    if (client_socket < 0) return;
    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    packet.payload.server.action = SERVER_LIST_MEMBERS;
    packet.payload.server.server_id = server_id;
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void request_channel_history(int64_t channel_id) {
    if (client_socket < 0) return;
    TizcordPacket packet = create_base_packet(PACKET_CHANNEL);
    packet.payload.channel.action = CHANNEL_HISTORY_REQUEST;
    packet.payload.channel.channel_id = channel_id;
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void send_channel_message(int64_t channel_id, const char *message) {
    if (client_socket < 0) return;
    
    TizcordPacket packet = create_base_packet(PACKET_CHANNEL);
    packet.payload.channel.action = CHANNEL_MESSAGE;
    packet.payload.channel.channel_id = channel_id;
    
    // Copy the text content
    strncpy(packet.payload.channel.message, message, MESSAGE_LEN - 1);
    
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void list_all_servers_request(void) {
    if (client_socket < 0) return;
    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    packet.payload.server.action = SERVER_LIST;
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void join_server(int64_t server_id) {
    if (client_socket < 0) return;
    TizcordPacket packet = create_base_packet(PACKET_SERVER);
    packet.payload.server.action = SERVER_JOIN;
    packet.payload.server.server_id = server_id;
    write(client_socket, &packet, sizeof(TizcordPacket));
}

void send_dm_message(int64_t recipient_id, const char *message) {
    if (client_socket < 0) return;
    
    TizcordPacket packet = create_base_packet(PACKET_DM);
    packet.payload.dm.action = DM_MESSAGE;
    packet.payload.dm.recipient_id = recipient_id;
    
    strncpy(packet.payload.dm.message, message, MESSAGE_LEN - 1);
    
    write(client_socket, &packet, sizeof(TizcordPacket));
}
