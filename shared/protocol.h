// This is the file that both server and client will include
// Prevents segfaults when packets being sent/received

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sqlite3.h>

typedef enum {
    MSG_LOGIN,
    MSG_DM,
    MSG_SERVER,
    MSG_CHANNEL,
    MSG_STATUS
} PacketType;

typedef enum { 
    STATUS_ONLINE, 
    STATUS_OFFLINE, 
    STATUS_AWAY 
} UserStatus;

// --- Action Flags ---
// These help the server know exactly what to do with the packet

typedef enum {
    AUTH_LOGIN,
    AUTH_REGISTER,
    AUTH_LOGOUT
} AuthAction;

typedef enum {
    DM_MESSAGE,
    DM_FRIEND_REQUEST,
    DM_FRIEND_ACCEPT,
    DM_MESSAGE_EDIT,
    DM_MESSAGE_DELETE
} DMAction;

typedef enum {
    SERVER_CREATE,
    SERVER_JOIN,
    SERVER_LEAVE
} ServerAction;

typedef enum {
    CHANNEL_CREATE,
    CHANNEL_DELETE,
    CHANNEL_JOIN,
    CHANNEL_MESSAGE,
    CHANNEL_MESSAGE_EDIT,
    CHANNEL_MESSAGE_DELETE 
} ChannelAction;

// --- Payloads ---

typedef struct {
    AuthAction action;
    int status_code;
    char username[32];
    char password[64]; // This should always be hashed
} AuthPayload;

typedef struct {
    DMAction action;
    int status_code;
    sqlite3_int64 message_id;
    char message[512]; // The actual text content
} DMPayload;

typedef struct {
    ServerAction action;
    sqlite3_int64 server_id;
    int status_code;
    char server_name[32];
} ServerPayload;

typedef struct {
    ChannelAction action;
    sqlite3_int64 server_id;
    int status_code;
    sqlite3_int64 channel_id;
    char channel_name[32];
    sqlite3_int64 message_id;
    char message[512];
} ChannelPayload;

typedef struct {
    UserStatus status; // for message status
} StatusPayload;

typedef struct {
    PacketType type;
    char sender[32];
    union {
        AuthPayload auth;
        DMPayload dm;
        ServerPayload server;
        ChannelPayload channel;
        StatusPayload status;
    } payload; 
    long timestamp;
    char receiver[32];
} TizcordPacket;

#endif