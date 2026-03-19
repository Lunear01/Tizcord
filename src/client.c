#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "protocol.h" // Ensure this is in your include path

#define PORT 58338

//TEST CLIENT
int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    TizcordPacket packet;

    // 1. Create socket
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Convert IPv4 address from text to binary (localhost)
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    // 2. Connect to server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // 3. Prepare the "Hello World" packet
    memset(&packet, 0, sizeof(TizcordPacket));
    packet.type = MSG_BROADCAST;
    strncpy(packet.content, "Hello World!", 30);
    packet.timestamp = (long)time(NULL);

    // 4. Send the packet
    // Note: We send the entire struct size so the server's read() matches
    if (send(sock_fd, &packet, sizeof(TizcordPacket), 0) < 0) {
        perror("Send failed");
    } else {
        printf("Sent: %s", packet.content);
    }

    close(sock_fd);
    return 0;
}