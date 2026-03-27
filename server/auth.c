#include "../include/auth.h"
#include "../include/protocol.h"
#include "../include/db.h"
#include "../include/server.h"

#include <crypt.h>
#include <stdio.h>
#include <string.h>

void register_account(ServerContext *ctx, int client_fd, TizcordPacket *packet) {
    printf("[Server] Received AUTH_REGISTER for %s\n", packet->payload.auth.username);
    
    TizcordPacket reply;
    memset(&reply, 0, sizeof(TizcordPacket));
    reply.type = MSG_LOGIN;
    reply.payload.auth.action = AUTH_REGISTER;
    
    printf("[Server] Step 1: Generating yescrypt salt...\n");
    char *setting = crypt_gensalt("$y$", 0, NULL, 0);
    if (setting == NULL) {
        printf("[Server] CRITICAL: crypt_gensalt returned NULL! Aborting hash.\n");
        reply.payload.auth.status_code = 1;
        write(client_fd, &reply, sizeof(TizcordPacket));
        return;
    }
    
    printf("[Server] Step 2: Hashing password...\n");
    char *hash = crypt(packet->payload.auth.password, setting);
    if (hash == NULL) {
        printf("[Server] CRITICAL: crypt returned NULL! Aborting hash.\n");
        reply.payload.auth.status_code = 1;
        write(client_fd, &reply, sizeof(TizcordPacket));
        return;
    }
    
    printf("[Server] Step 3: Hashing success! Inserting into Database...\n");
    if (ctx == NULL || ctx->db == NULL) {
        printf("[Server] CRITICAL: Database context is NULL! Cannot insert.\n");
        reply.payload.auth.status_code = 1;
        write(client_fd, &reply, sizeof(TizcordPacket));
        return;
    }
    
    char user_id[37];
    int rc = db_create_user(ctx->db, packet->payload.auth.username, hash, user_id);
    
    if (rc == 0) {
        printf("[Server] Successfully registered user! UUID: %s\n", user_id);
        reply.payload.auth.status_code = 0;
    } else {
        printf("[Server] Failed to register user (Username likely already exists!).\n");
        reply.payload.auth.status_code = 1;
    }
    
    // Send standard server response packet back
    write(client_fd, &reply, sizeof(TizcordPacket));
}

void handle_auth_packet(ServerContext *ctx, int client_fd, TizcordPacket *packet) {
    if (packet->payload.auth.action == AUTH_REGISTER) {
        register_account(ctx, client_fd, packet);
    }
}