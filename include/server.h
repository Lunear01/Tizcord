//Server header file for a simple chat server application

#define MAX_CLIENTS 100
#include <stdbool.h>

typedef struct {
    int socket_fd;
    char username[32];
    int current_channel_id;
    bool is_authenticated;
} ClientNode;

typedef struct {
    ClientNode clients[MAX_CLIENTS];
    int server_fd;
    int client_count;
} ServerContext;

void init_server_context(ServerContext *ctx);
int start_server(int port);
void check_session(); 
void run_server_loop(ServerContext *ctx);
void handle_new_connection(ServerContext *ctx);
int accept_connection(int listenfd);
