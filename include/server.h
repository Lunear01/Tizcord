// Server header file for a simple chat server application

#ifndef SERVER_H
#define SERVER_H

#define MAX_CLIENTS 64
#define MAX_CHANNELS 16
#define MAX_SERVERS 16
#define MAX_SERVER_MEMBERS 64
#define MAX_NAME_LEN 32
#define UUID_LEN 37
#define BUFFER_SIZE 1024

#include <stdbool.h>

#include "db.h"

typedef struct {
    char id[UUID_LEN];
    char name[MAX_NAME_LEN];
    int member_fds[MAX_SERVER_MEMBERS];
    int member_count;
} Channel;

typedef struct {
    char id[UUID_LEN];
    char name[MAX_NAME_LEN];
    Channel channels[MAX_CHANNELS];
    int channel_count;
    int member_fds[MAX_SERVER_MEMBERS];
    int member_count;
} TizcordServer;

typedef struct {
    int server_idx;
    int channel_idx;
} TizcordServerMembership;

typedef struct {
    char id[UUID_LEN];
    int socket_fd;
    bool is_authenticated;
    char username[MAX_NAME_LEN];
    TizcordServerMembership tizcord_membership[MAX_SERVERS];
    int active_tizcord_server;
    struct ServerContext* ctx; // Added pointer to parent context
} ClientNode;

typedef struct ServerContext { // Named the struct properly for pointer access
    int server_fd;
    int client_count;
    ClientNode clients[MAX_CLIENTS];
    TizcordServer tizcord_servers[MAX_SERVERS];
    int tizcord_server_count;
    DbContext* db;
} ServerContext;

typedef enum { STATUS_ONLINE, STATUS_OFFLINE, STATUS_AWAY } UserStatus;

/* Server lifecycle */
void init_server_context(ServerContext* ctx);
int start_server(int port);
void run_server_loop(ServerContext* ctx);

/* handle connections */
void handle_new_connection(ServerContext* ctx);
void handle_client_message(ServerContext* ctx, int client_index);

/* channel */
int find_or_create_channel(ServerContext* ctx, const char* name);
void channel_broadcast(ServerContext* ctx, int chan_idx, const char* msg, int len, int sender_fd);

/* tizcord server */
void find_or_create_tizcord_server(ServerContext* ctx, const char* server_name);

/* user operations */
void update_user_status(ServerContext* ctx, int client_index, UserStatus new_status);
void send_private_message(ServerContext* ctx, int sender_index, const char* recipient_username,
                          const char* msg, int len);

/* safe send */
int safe_send(int fd, const char* msg, int len);

#endif