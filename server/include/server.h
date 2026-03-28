// Server header file for a simple chat server application

#ifndef SERVER_H
#define SERVER_H

#define MAX_CLIENTS 64
#define MAX_CHANNELS 16
#define MAX_SERVERS 16
#define MAX_SERVER_MEMBERS 64
#define MAX_NAME_LEN 32
#define BUFFER_SIZE 1024

#include <stdbool.h>
#include <sqlite3.h>

#include "db.h"
#include "../../shared/protocol.h"

typedef struct {
    sqlite3_int64 id;
    char name[MAX_NAME_LEN];
    int member_fds[MAX_SERVER_MEMBERS];
    int member_count;
} Channel;

typedef struct {
    sqlite3_int64 id;
    char name[MAX_NAME_LEN];
    Channel channels[MAX_CHANNELS];
    int channel_count;
    int member_fds[MAX_SERVER_MEMBERS];
    int member_count;
} TizcordServer;

typedef struct {
    TizcordServer* server;
    bool is_admin;
} TizcordServerMembership;

typedef struct {
    sqlite3_int64 id;
    int socket_fd;
    bool is_authenticated;
    char username[MAX_NAME_LEN];
    TizcordServerMembership membership[MAX_SERVERS]; 
    int joined_server_count;
    int current_server_index;
    struct ServerContext* ctx;
} ClientNode;

typedef struct ServerContext {
    int server_fd;
    int client_count;
    ClientNode clients[MAX_CLIENTS];
    TizcordServer tizcord_servers[MAX_SERVERS];
    int tizcord_server_count;
    DbContext* db;
} ServerContext;


/* Server lifecycle */
void init_server_context(ServerContext* ctx, DbContext* db);
int start_server(int port);
void run_server_loop(ServerContext* ctx, int server_socket);

/* handle connections */
void handle_new_connection(ServerContext* ctx, int server_socket);
void *client_handler(void* arg);

#endif