#include "../shared/protocol.h"
#include "include/server.h"
#include "include/auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

// Reset everything in the server
void init_server_context(ServerContext *ctx, DbContext* db) {
    ctx->client_count = 0;
    ctx->tizcord_server_count = 0;
    memset(ctx->clients, 0, sizeof(ctx->clients));
    memset(ctx->tizcord_servers, 0, sizeof(ctx->tizcord_servers));
    ctx->tizcord_server_count = 0;
    ctx->db = db;
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
    int on = 1;
    int status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                            (const char *) &on, sizeof(on));
    if (status == -1) {
        perror("setsockopt -- REUSEADDR");
    }

    // setup server address structure and bind
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

    // Bind socket to address
    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Set up a queue in the kernel to hold pending connections. 
    if (listen(socket_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on port %d\n", port);
    return socket_fd;
}

void run_server_loop(ServerContext *ctx, int server_socket) {
    fd_set master_set, read_set, write_set;
    FD_ZERO(&master_set);
    FD_SET(server_socket, &master_set);
    int max_fd = server_socket;

    while (1) {
        read_set = master_set;
        write_set = master_set;
        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < ctx->client_count; i++) {
            int client_fd = ctx->clients[i].socket_fd;
            if (FD_ISSET(client_fd, &read_set)) {
                // Handle client communication in a separate thread
            }
        }
    }
}

// Handle new connections
void handle_new_connection(ServerContext *ctx, int server_socket) {
    int client_fd = accept_connection(server_socket);
    if (client_fd < 0) {
        return; // accept failed, just return to the main loop
    }
	
	if (ctx->client_count >= MAX_CLIENTS) {
		printf("Server full! Rejecting incoming connection.\n");
		close(client_fd);
		return;
	}

	ClientNode *client = malloc(sizeof(ClientNode));
    if (!client) {
        perror("malloc");
        close(client_fd);
        return;
    }

    memset(client, 0, sizeof(ClientNode));
    client->socket_fd = client_fd;
    client->ctx = ctx;

    ctx->clients[ctx->client_count++] = *client;
}

// Accept a new connection and return the client socket fd
int accept_connection(int listenfd) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;

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
}
