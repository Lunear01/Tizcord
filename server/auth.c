#include "../shared/protocol.h"
#include "include/auth.h"
#include "include/db.h"
#include "include/server.h"
#include "include/tizcord_social.h"
#include "include/tizcord_server.h"

#include <crypt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void clear_client_session(ClientNode *client) {
    if (client == NULL) {
        return;
    }

    client->is_authenticated = 0;
    client->id = 0;
    client->username[0] = '\0';
    memset(client->membership, 0, sizeof(client->membership));
    client->joined_server_count = 0;
    client->current_server_index = -1;
}

static void revoke_existing_login(ServerContext *ctx, int current_client_fd,
                                  int64_t user_id, const char *username) {
    if (ctx == NULL) {
        return;
    }

    for (int i = 0; i < ctx->client_count; i++) {
        ClientNode *other = &ctx->clients[i];

        if (other->socket_fd <= 0 || other->socket_fd == current_client_fd) {
            continue;
        }
        if (!other->is_authenticated || other->id != user_id) {
            continue;
        }

        TizcordPacket logout_packet;
        memset(&logout_packet, 0, sizeof(TizcordPacket));
        logout_packet.type = PACKET_AUTH;
        logout_packet.payload.auth.action = AUTH_LOGOUT;
        logout_packet.payload.auth.status_code = RESP_OK;
        if (username != NULL) {
            strncpy(logout_packet.payload.auth.username, username, MAX_NAME_LEN - 1);
        }

        write(other->socket_fd, &logout_packet, sizeof(TizcordPacket));
        clear_client_session(other);
        printf("[Server] Revoked older session for %s on fd %d\n",
               username != NULL ? username : "(unknown)", other->socket_fd);
    }
}

void register_account(ServerContext *ctx, int client_fd, TizcordPacket *packet) {
    printf("[Server] Received AUTH_REGISTER for %s\n", packet->payload.auth.username);
    
    TizcordPacket reply;
    memset(&reply, 0, sizeof(TizcordPacket));
    reply.type = PACKET_AUTH;
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

        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].socket_fd == client_fd) {
                ctx->clients[i].is_authenticated = 1;
                ctx->clients[i].id = user_id;
                strncpy(ctx->clients[i].username, packet->payload.auth.username, sizeof(ctx->clients[i].username) - 1);
                break;
            }
        }

        notify_all_user_lists(ctx);
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
    reply.type = PACKET_AUTH;
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

        revoke_existing_login(ctx, client_fd, db_user_id, packet->payload.auth.username);
        
        // Link identity into active client memory
        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].socket_fd == client_fd) {
                ctx->clients[i].is_authenticated = 1;
                ctx->clients[i].id = db_user_id;
                strncpy(ctx->clients[i].username, packet->payload.auth.username, sizeof(ctx->clients[i].username) - 1);
                break;
            }
        }

        notify_server_member_lists_for_user(ctx, db_user_id);
        notify_all_user_lists(ctx);
    }
    
    write(client_fd, &reply, sizeof(TizcordPacket));
}

void logout_account(ServerContext *ctx, int client_fd, TizcordPacket *packet) {
    (void)packet;

    if (ctx == NULL) {
        return;
    }

    for (int i = 0; i < ctx->client_count; i++) {
        ClientNode *client = &ctx->clients[i];

        if (client->socket_fd != client_fd) {
            continue;
        }

        if (!client->is_authenticated) {
            TizcordPacket reply;
            memset(&reply, 0, sizeof(TizcordPacket));
            reply.type = PACKET_AUTH;
            reply.payload.auth.action = AUTH_LOGOUT;
            reply.payload.auth.status_code = RESP_ERR_UNAUTHORIZED;
            write(client_fd, &reply, sizeof(TizcordPacket));
            return;
        }

        int64_t user_id = client->id;
        char username[MAX_NAME_LEN] = {0};
        strncpy(username, client->username, sizeof(username) - 1);

        clear_client_session(client);
        notify_server_member_lists_for_user(ctx, user_id);
        notify_all_user_lists(ctx);

        TizcordPacket reply;
        memset(&reply, 0, sizeof(TizcordPacket));
        reply.type = PACKET_AUTH;
        reply.payload.auth.action = AUTH_LOGOUT;
        reply.payload.auth.status_code = RESP_DONE;
        strncpy(reply.payload.auth.username, username, MAX_NAME_LEN - 1);

        write(client_fd, &reply, sizeof(TizcordPacket));
        printf("[Server] Logged out user %s on fd %d\n", username, client_fd);
        return;
    }
}

void handle_auth_packet(ServerContext *ctx, int client_fd, TizcordPacket *packet) {
    if (packet->payload.auth.action == AUTH_REGISTER) {
        register_account(ctx, client_fd, packet);
    } else if (packet->payload.auth.action == AUTH_LOGIN) {
        login_account(ctx, client_fd, packet);
    } else if (packet->payload.auth.action == AUTH_LOGOUT) {
        logout_account(ctx, client_fd, packet);
    }
}
