#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "../include/client.h"
#include "../include/ui.h"
#include "protocol.h"

// Access the global socket defined in client.c
extern int client_socket;

// Global mutex for thread-safe UI updates
pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;

// Background thread to continuously listen for server packets
void* server_listener(void* arg) {
    (void)arg;
    TizcordPacket packet;
    
    // Read continuously as long as the socket is open and returning data
    while (client_socket != -1 && read(client_socket, &packet, sizeof(TizcordPacket)) > 0) {
        
        // Lock the UI before reading or modifying any shared state
        pthread_mutex_lock(&ui_mutex);
        
        switch (packet.type) {
            case CHANNEL:
                if (packet.payload.channel.action == CHANNEL_MESSAGE) {
                    // Inject the incoming channel message into the UI state
                    ui_receive_channel_message(&packet);
                } 
                else if (packet.payload.channel.action == CHANNEL_MESSAGE_EDIT) {
                    ui_edit_channel_message(&packet);
                }
                else if (packet.payload.channel.action == CHANNEL_MESSAGE_DELETE) {
                    ui_delete_channel_message(&packet);
                }
                break;
                
            case DM:
                if (packet.payload.dm.action == DM_MESSAGE) {
                    // Route the DM to the UI
                    ui_receive_dm_message(&packet);
                }
                break;
                
            case SERVER:
                // Handle new server creation broadcasts, joins, etc.
                ui_update_server_state(&packet);
                break;
                
            case AUTH:
                // Handle auth responses (if any are sent asynchronously outside the blocking login function)
                ui_handle_auth_response(&packet);
                break;
                
            default:
                break;
        }
        
        // Force NCURSES to redraw the current screen so the user sees the new message immediately
        ui_force_redraw();
        
        // Unlock the UI so the main thread can process user input again
        pthread_mutex_unlock(&ui_mutex);
    }
    
    printf("\n[Client] Disconnected from server.\n");
    return NULL;
}

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

    // Spawn a listener thread to handle incoming server responses
    pthread_t listener_tid;
    if (pthread_create(&listener_tid, NULL, server_listener, NULL) != 0) {
        perror("Failed to create server listener thread");
        close(client_socket);
        return EXIT_FAILURE;
    }
    
    // Detach so we don't have to join it later; it will die when the process exits
    pthread_detach(listener_tid);

    // Pass control to the NCURSES User Interface!
    start_ui();

    // Clean up when the UI loop breaks (user quits)
    if (client_socket != -1) {
        close(client_socket);
    }
    
    // Clean up the mutex
    pthread_mutex_destroy(&ui_mutex);
    
    return 0;
}