// This is the file that both server and client will include
// Prevents segfaults when packets being sent/received

#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef enum {
    MSG_LOGIN,
    MSG_DM,
    MSG_SERVER,
    MSG_CHANNEL,
} PacketType;

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
    DM_FRIEND_ACCEPT
} DMAction;

typedef enum {
    SERVER_CREATE,
    SERVER_JOIN,
    SERVER_LEAVE
} ServerAction;

typedef enum {
    CHANNEL_CREATE,
    CHANNEL_DELETE,
    CHANNEL_JOIN
} ChannelAction;

// --- Payloads ---

typedef struct {
    AuthAction action;
    int status_code;
    char username[32];
    char password[64]; // In a real app, ensure this is hashed!
} AuthPayload;

typedef struct {
    DMAction action;
    char message[512]; // The actual text content
} DMPayload;

typedef struct {
    ServerAction action;
    int server_id;
    char server_name[32];
} ServerPayload;

typedef struct {
    ChannelAction action;
    int server_id;
    int channel_id;
    char channel_name[32];
} ChannelPayload;


typedef struct {
    PacketType type;
    char sender[32];
    union {
        AuthPayload auth;
        DMPayload dm;
        ServerPayload server;
        ChannelPayload channel;
    } payload; 
    long timestamp;
    char receiver[32];
} TizcordPacket;

#endif