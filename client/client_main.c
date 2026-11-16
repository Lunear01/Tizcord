#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../shared/tls.h"
#include "include/client.h"
#include "include/ui.h"

// Access the global socket defined in client.c
extern int client_socket;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip_address> <port> (<server.crt> | --insecure)\n", argv[0]);
        fprintf(stderr, "  server.crt is the server's certificate, pinned as the only trusted CA.\n");
        return EXIT_FAILURE;
    }

    const char *ip_address = argv[1];
    int port = atoi(argv[2]);

    if (strcmp(argv[3], "--insecure") == 0) {
        fprintf(stderr, "WARNING: TLS disabled, traffic is plaintext\n");
    } else if (tls_init_client(argv[3]) != 0) {
        fprintf(stderr, "Fatal: Could not load %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    connect_to_server(ip_address, port);

    // Ensure we actually connected before starting the UI
    if (client_socket < 0) {
        fprintf(stderr, "Fatal: Could not connect to server.\n");
        return EXIT_FAILURE;
    }

    start_ui();

    // Gentle closing connection
    if (client_socket != -1) {
        tls_close(client_socket);
        close(client_socket);
    }
    tls_cleanup();
    
    return 0;
}