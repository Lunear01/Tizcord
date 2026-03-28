// This is the file that both server and client will include
// Prevents segfaults when packets being sent/received

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sqlite3.h>

#define MAX_PACKET_SIZE 1024
#define MAX_NAME_LEN 32
#define PASSWORD_LEN 64
#define MESSAGE_LEN 512

typedef enum {
    AUTH,
    DM,
    SERVER,
    CHANNEL,
    SOCIAL,
    STATUS
} PacketType;

typedef enum { 
    STATUS_ONLINE, 
    STATUS_OFFLINE, 
    STATUS_AWAY 
} UserStatus;

// --- Action Flags ---
// These help the server know exactly what to do with the packet
typedef enum {
    SYSTEM_PING,
    SYSTEM_PONG,
    SYSTEM_ERROR
} SystemAction;

typedef enum {
    AUTH_LOGIN,
    AUTH_REGISTER,
    AUTH_LOGOUT
} AuthAction;

typedef enum {
    DM_MESSAGE,
    DM_MESSAGE_EDIT,
    DM_MESSAGE_DELETE,
} DMAction;

typedef enum {
    SERVER_JOIN,
    SERVER_LEAVE,
    SERVER_CREATE,
    SERVER_DELETE,   // admin only
    SERVER_EDIT,     // admin only
    SERVER_LIST,
    SERVER_LIST_CHANNELS,
    SERVER_LIST_MEMBERS,
    SERVER_LIST_JOINED,
    SERVER_KICK_MEMBER,  // admin only 
} ServerAction;

typedef enum {
    CHANNEL_CREATE, // admin only
    CHANNEL_DELETE, // admin only
    CHANNEL_JOIN,
    CHANNEL_MESSAGE,
    CHANNEL_MESSAGE_EDIT,
    CHANNEL_MESSAGE_DELETE, 
    CHANNEL_HISTORY_REQUEST,
} ChannelAction;

typedef enum {
    SOCIAL_FRIEND_REQUEST,
    SOCIAL_FRIEND_ACCEPT,
    SOCIAL_FRIEND_REJECT,
    SOCIAL_FRIEND_REMOVE,
    SOCIAL_LIST_FRIENDS,
    SOCIAL_CHECK_ONLINE,
    SOCIAL_UPDATE_STATUS
} SocialAction;

// --- Payloads ---

typedef struct {
    AuthAction action;
    int status_code;
    char username[MAX_NAME_LEN];
    char password[PASSWORD_LEN]; // This should always be hashed
} AuthPayload;

typedef struct {
    DMAction action;
    int status_code;
    int recipient_id; // user ID of the recipient 
    sqlite3_int64 message_id;
    char message[MESSAGE_LEN]; // The actual text content
} DMPayload;

typedef struct {
    ServerAction action;
    sqlite3_int64 server_id;
    int status_code;
    char server_name[MAX_NAME_LEN];
} ServerPayload;

typedef struct {
    ChannelAction action;
    sqlite3_int64 server_id;
    int status_code;
    sqlite3_int64 channel_id;
    char channel_name[MAX_NAME_LEN];
    sqlite3_int64 message_id;
    char message[MESSAGE_LEN];
} ChannelPayload;

typedef struct {
    UserStatus status; // for message status
} StatusPayload;

typedef struct {
    PacketType type;
    sqlite3_int64 sender_id;
    union {
        AuthPayload auth;
        DMPayload dm;
        ServerPayload server;
        ChannelPayload channel;
        StatusPayload status;
    } payload; 
    long long timestamp;
} TizcordPacket;

#endif