#include "../include/server.h"
#include "../include/protocol.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>


// Reset everything in the server
void init_server_context(ServerContext *ctx) {
    ctx->client_count = 0;
    ctx->server_fd = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ctx->clients[i].socket_fd = -1;
        ctx->clients[i].is_active = false;
        ctx->clients[i].is_authenticated = false;
    }
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
}

// Handle new connections
void handle_new_connection(ServerContext *ctx) {
   
}

// Run forever and call handle_new_connection
void run_server_loop(ServerContext *ctx) {

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
