#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/client.h"
#include "../include/ui.h"

// Access the global socket defined in client.c
extern int client_socket;

int main(int argc, char *argv[]) {
    // Check if both IP address and port are provided
    if (argc < 3) {
        fprintf(stderr, "No IP address or port given.\n");
        fprintf(stderr, "Usage: %s [ip_address] [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse command line arguments
    const char *ip_address = argv[1];
    int port = atoi(argv[2]);

    connect_to_server(ip_address, port);

    // Ensure we actually connected before starting the UI
    if (client_socket < 0) {
        fprintf(stderr, "Fatal: Could not connect to server.\n");
        return EXIT_FAILURE;
    }

    start_ui();

    // Gentle closing connection
    if (client_socket != -1) {
        close(client_socket);
    }
    
    return 0;
}