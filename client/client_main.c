#include <stdio.h>
#include <unistd.h>
#include "../include/client.h"
#include "../include/ui.h"

int main() {
    //connect to server


    //run the ui and run auth methods

    //make ui monitor for user actions

    //feed user actions into client.c 

    //update ui from server responses

    printf("Starting Client and Connecting to 127.0.0.1:4242...\n");
    connect_to_server("127.0.0.1", 4242);
    
    // Pass control to the NCURSES User Interface!
    start_ui();
    
    return 0;
}