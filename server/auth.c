#include "../shared/protocol.h"
#include "include/auth.h"
#include "include/db.h"
#include "include/server.h"

#include <crypt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void register_account(ServerContext *ctx, int client_fd, TizcordPacket *packet) {
    printf("[Server] Received AUTH_REGISTER for %s\n", packet->payload.auth.username);
    
    TizcordPacket reply;
    memset(&reply, 0, sizeof(TizcordPacket));
    reply.type = AUTH;
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
    
    int64_t user_id;
    int rc = db_create_user(ctx->db, packet->payload.auth.username, hash, &user_id);
    
    if (rc == 0) {
        printf("[Server] Successfully registered user! ID: %lld\n", (long long)user_id);
        reply.payload.auth.status_code = 0;
    } else {
        printf("[Server] Failed to register user (Username likely already exists!).\n");
        reply.payload.auth.status_code = 1;
    }
    
    // Send standard server response packet back
    write(client_fd, &reply, sizeof(TizcordPacket));
}

void login_account(ServerContext *ctx, int client_fd, TizcordPacket *packet) {
    printf("[Server] Received AUTH_LOGIN for %s\n", packet->payload.auth.username);
    
    TizcordPacket reply;
    memset(&reply, 0, sizeof(TizcordPacket));
    reply.type = AUTH;
    reply.payload.auth.action = AUTH_LOGIN;
    
    if (ctx == NULL || ctx->db == NULL) {
        printf("[Server] CRITICAL: DB context is NULL!\n");
        reply.payload.auth.status_code = 1;
        write(client_fd, &reply, sizeof(TizcordPacket));
        return;
    }
    
    char db_hash[256] = {0};
    int64_t db_user_id = 0;
    int rc = db_get_password_hash(ctx->db, packet->payload.auth.username, db_hash, &db_user_id);
    
    if (rc != 0) {
        printf("[Server] Login Failed: Username %s not found.\n", packet->payload.auth.username);
        reply.payload.auth.status_code = 1;
        write(client_fd, &reply, sizeof(TizcordPacket));
        return;
    }
    
    char *computed_hash = crypt(packet->payload.auth.password, db_hash);
    if (computed_hash == NULL || strcmp(computed_hash, db_hash) != 0) {
        printf("[Server] Login Failed: Incorrect password for %s.\n", packet->payload.auth.username);
        reply.payload.auth.status_code = 1;
    } else {
        printf("[Server] Successfully authenticated user %s! (ID: %lld)\n", 
               packet->payload.auth.username, (long long)db_user_id);
        reply.payload.auth.status_code = 0;
        
        // Link identity into active client memory
        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].socket_fd == client_fd) {
                ctx->clients[i].is_authenticated = 1;
                ctx->clients[i].id = db_user_id;
                strncpy(ctx->clients[i].username, packet->payload.auth.username, sizeof(ctx->clients[i].username) - 1);
                break;
            }
        }
    }
    
    write(client_fd, &reply, sizeof(TizcordPacket));
}

void handle_auth_packet(ServerContext *ctx, int client_fd, TizcordPacket *packet) {
    if (packet->payload.auth.action == AUTH_REGISTER) {
        register_account(ctx, client_fd, packet);
    } else if (packet->payload.auth.action == AUTH_LOGIN) {
        login_account(ctx, client_fd, packet);
    }
}