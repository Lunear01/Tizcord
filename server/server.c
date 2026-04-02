#include "../shared/protocol.h"
#include "include/server.h"
#include "include/auth.h"
#include "include/tizcord_server.h"
#include "include/tizcord_chat.h"
#include "include/tizcord_social.h"


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


void run_server_loop(ServerContext *ctx) {
    fd_set master_set, read_set;
    FD_ZERO(&master_set);
    FD_SET(ctx->server_fd, &master_set);
    int max_fd = ctx->server_fd;

    while (1) {
        read_set = master_set;
        
        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // handle new connection
        if (FD_ISSET(ctx->server_fd, &read_set)) {
            int new_client_fd = handle_new_connection(ctx);
            if (new_client_fd != -1) {
                FD_SET(new_client_fd, &master_set);
                if (new_client_fd > max_fd) {
                    max_fd = new_client_fd;
                }
            }
        }

        // loop through each client
        for (int i = 0; i < ctx->client_count; i++) {
            int client_fd = ctx->clients[i].socket_fd;
            if (client_fd > 0 && FD_ISSET(client_fd, &read_set)) {
                TizcordPacket packet;
                ssize_t bytes_received = read(client_fd, &packet, sizeof(TizcordPacket));
                if (bytes_received <= 0) {
                    // Client disconnected or error
                    int64_t disconnected_user_id = 0;
                    int was_authenticated = ctx->clients[i].is_authenticated;
                    if (was_authenticated) {
                        disconnected_user_id = ctx->clients[i].id;
                    }

                    printf("Client on fd %d disconnected.\n", client_fd);
                    close(client_fd);
                    FD_CLR(client_fd, &master_set);
                    
                    // remove client
                    for (int j = i; j < ctx->client_count - 1; j++) {
                        ctx->clients[j] = ctx->clients[j + 1];
                    }
                    ctx->client_count--;
                    i--; 

                    if (was_authenticated && disconnected_user_id > 0) {
                        notify_server_member_lists_for_user(ctx, disconnected_user_id);
                        notify_all_user_lists(ctx);
                    }
                } else {
                    // Process the received packet
                    process_client_packet(ctx, &ctx->clients[i], &packet);
                }
            }
        }
    }
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

// Handle new connections
int handle_new_connection(ServerContext *ctx) {
    int client_fd = accept_connection(ctx->server_fd);
    if (client_fd < 0) {
        return -1; // accept failed, just return to the main loop
    }
	
	if (ctx->client_count >= MAX_CLIENTS) {
		printf("Server full! Rejecting incoming connection.\n");
		close(client_fd);
		return -1;
	}

	ClientNode *client = &ctx->clients[ctx->client_count];
    memset(client, 0, sizeof(ClientNode));
    client->socket_fd = client_fd;
    client->current_server_index = -1;
    client->ctx = ctx;
    ctx->clients[ctx->client_count++] = *client;
    return client_fd;
}

// Handle individual client communication
void process_client_packet(ServerContext *ctx, ClientNode *client, TizcordPacket *packet) {
    switch (packet->type) {
        case PACKET_AUTH:
            printf("[Server] Received PACKET_AUTH packet\n");
            handle_auth_packet(ctx, client->socket_fd, packet);
            break;
        case PACKET_DM:
            printf("[Server] Received PACKET_DM packet\n");
            handle_chat_packet(ctx, packet, client->socket_fd);
            break;
        case PACKET_SERVER:
            printf("[Server] Received PACKET_SERVER packet\n");
            handle_server_packet(ctx, client, packet);
            break;
        case PACKET_CHANNEL:
            printf("[Server] Received PACKET_CHANNEL packet\n");
            handle_chat_packet(ctx, packet, client->socket_fd);
            break;
        case PACKET_SOCIAL :
            printf("[Server] Received PACKET_SOCIAL packet\n");
            handle_social_packet(ctx, client, packet);
            break;
        default:
            printf("[Server] Unknown packet type %d received!\n", packet->type);
            break;
    }

    return;
}
