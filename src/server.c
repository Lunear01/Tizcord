#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

void start_server(int port) {
    // Initialize server context and set up socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    struct sockaddr_in server_addr;
    // Initialize the server address structure to zero
    memset(&server_addr, 0, sizeof(server_addr));

    // Configure the server address structure
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
}

void handle_new_connection(ServerContext *ctx) {

}
void broadcast_message(ServerContext *ctx, char *msg, int sender_fd) {

}