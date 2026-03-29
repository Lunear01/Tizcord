#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/client.h"
#include "../include/ui.h"

// Access the global socket defined in client.c
extern int client_socket;

int main(int argc, char *argv[]) {
    // Default connection settings
    const char *ip_address = "127.0.0.1";
    int port = 4242;

    // Parse command line arguments if provided
    if (argc >= 2) {
        ip_address = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }

    printf("Starting Client and Connecting to %s:%d...\n", ip_address, port);
    connect_to_server(ip_address, port);

    // Ensure we actually connected before starting the UI
    if (client_socket < 0) {
        fprintf(stderr, "Fatal: Could not connect to server.\n");
        return EXIT_FAILURE;
    }

    // Pass control to the NCURSES User Interface!
    start_ui();

    // Clean up when the UI loop breaks (user quits)
    if (client_socket != -1) {
        close(client_socket);
    }
    
    return 0;
}