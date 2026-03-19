#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


#include "../include/server.h"
#include "../include/protocol.h" 
#define BUFSIZE sizeof(TizcordPacket)

int main() {
    ServerContext ctx;
    init_server_context(&ctx);
    //TEST start_server works
    ctx.server_fd = start_server(58338);

    printf("Waiting for a single connection to print raw data...\n");

    // TEST: Accept only ONE connection
    int fd = accept_connection(ctx.server_fd);
    if (fd < 0) {
        perror("Accept failed");
        close(ctx.server_fd);
        return 1;
    }

    char buf[BUFSIZE];
    int nbytes;

    // Read and print raw data until the client disconnects
    while ((nbytes = read(fd, buf, BUFSIZE - 1)) > 0) {
        printf("Received %d bytes: ", nbytes);
        
        for (int i = 0; i < nbytes; i++) {
            printf("%c", buf[i]);
        } 
        
        printf("\n");
    }

    if (nbytes == 0) {
        printf("\nClient disconnected. Closing server.\n");
    } else if (nbytes < 0) {
        perror("Read error");
    }

    close(fd);
    close(ctx.server_fd);
    return 0;
}

