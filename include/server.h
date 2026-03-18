//Server header file for a simple chat server application

#define MAX_CLIENTS 100

typedef struct {
    int socket_fd;
    char username[32];
    int current_channel_id;
    bool is_active;
    bool is_authenticated;
} ClientNode;

typedef struct {
    ClientNode clients[MAX_CLIENTS];
    int server_fd;
} ServerContext;

void start_server(int port);
void handle_new_connection(ServerContext *ctx);
void broadcast_message(ServerContext *ctx, char *msg, int sender_fd);