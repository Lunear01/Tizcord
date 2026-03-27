#include "../include/server.h"
#include "../include/protocol.h" 
#include "../include/auth.h"
#include "../include/db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

// Reset everything in the server
void init_server_context(ServerContext *ctx) {
    ctx->client_count = 0;
    ctx->tizcord_server_count = 0;
    memset(ctx->clients, 0, sizeof(ctx->clients));
    memset(ctx->tizcord_servers, 0, sizeof(ctx->tizcord_servers));
    ctx->tizcord_server_count = 0;
    ctx->db = db_connect("tizcord.db");
}

// Start server at a port ready to listen
int start_server(int port) {
    
    // Initialize socket for listening
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse the address/port immediately after restart
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    
    if (listen(socket_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on port %d\n", port);
    return socket_fd;
}


// Handle individual client communication
void *client_handler(void *arg) {
    ClientNode *client = (ClientNode *)arg;
    TizcordPacket packet;
    
    // Read packets in a loop
    while (read(client->socket_fd, &packet, sizeof(TizcordPacket)) > 0) {
        switch (packet.type) {
            case MSG_LOGIN:
                handle_auth_packet(client->ctx, client->socket_fd, &packet);
                break;
                
            case MSG_DM:
            case MSG_CHANNEL:
                // Route chat and channel messages to the chat implementation
                handle_chat_packet(client->ctx, &packet, client->socket_fd);
                break;
                
            case MSG_SERVER:
                printf("[Server] Received SERVER packet (Routing not yet implemented)\n");
                // TODO: Route to server/channel creation logic
                break;
                
            case MSG_STATUS:
                printf("[Server] Received STATUS packet (Routing not yet implemented)\n");
                // TODO: Update client array status and broadcast to friends
                break;
                
            default:
                printf("[Server] Unknown packet type received!\n");
                break;
    }
    
    close(client->socket_fd);
    printf("Client disconnected.\n");
    return NULL;
}

// Handle new connections
void handle_new_connection(ServerContext *ctx) {
    int client_fd = accept_connection(ctx->server_fd);
    if (client_fd < 0) return;
    
    if (ctx->client_count >= MAX_CLIENTS) {
        printf("Server full! Rejecting incoming connection.\n");
        close(client_fd);
        return;
    }
    
    // Grab the next available static ClientNode slots
    ClientNode *client = &ctx->clients[ctx->client_count++];
    client->socket_fd = client_fd;
    client->ctx = ctx; 
    
    pthread_t tid;
    pthread_create(&tid, NULL, client_handler, client);
    pthread_detach(tid);
}

// Run forever and call handle_new_connection
void run_server_loop(ServerContext *ctx) {
    while (1) {
        handle_new_connection(ctx);
    }
}

// TEST CODE FROM LAB 10
int accept_connection(int listenfd) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;

    fprintf(stderr, "Waiting for a new connection...\n");
    int client_socket = accept(listenfd, (struct sockaddr *)&peer, &peer_len);
    if (client_socket < 0) {
        perror("accept");
        return -1;
    } else {
        fprintf(stderr,
            "New connection accepted from %s:%d\n",
            inet_ntoa(peer.sin_addr),
            ntohs(peer.sin_port));
        return client_socket;
    }
}
