// This is the file that both server and client will include
// Prevents segfaults when packets being sent/received

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_PACKET_SIZE 1024
#define MAX_NAME_LEN 32
#define PASSWORD_LEN 64
#define MESSAGE_LEN 512
#define SYSTEM_MESSAGE_LEN 256
#define PROFILE_STATUS_LEN 64

typedef enum {
    PACKET_AUTH,
    PACKET_DM,
    PACKET_SERVER,
    PACKET_CHANNEL,
    PACKET_SOCIAL,
} PacketType;

typedef enum { 
    STATUS_ONLINE, 
    STATUS_OFFLINE
} UserStatus;

typedef enum {
    RESP_OK = 0,
    RESP_DONE = 1,
    RESP_ERR_INVALID = -1,
    RESP_ERR_UNAUTHORIZED = -2,
    RESP_ERR_NOT_FOUND = -3,
    RESP_ERR_CONFLICT = -4,
    RESP_ERR_DB = -5,
    RESP_ERR_INTERNAL = -6,
} ResponseCode;

typedef enum {
    LIST_FRAME_SINGLE,
    LIST_FRAME_START, 
    LIST_FRAME_MIDDLE,
    LIST_FRAME_END 
} ListFrameType;

typedef enum {
    SYSTEM_PING,
    SYSTEM_PONG,
    SYSTEM_ERROR
} SystemAction;

// Action Flags
// These help the server know exactly what to do with the packet

typedef enum {
    AUTH_LOGIN,
    AUTH_REGISTER,
    AUTH_LOGOUT
} AuthAction;

typedef enum {
    DM_MESSAGE,
    DM_HISTORY_REQUEST
} DMAction;

typedef enum {
    SERVER_JOIN,
    SERVER_LEAVE,
    SERVER_CREATE,
    SERVER_DELETE,   // admin only
    SERVER_LIST,
    SERVER_LIST_CHANNELS,
    SERVER_LIST_MEMBERS,
    SERVER_LIST_JOINED,
    SERVER_KICK_MEMBER,  // admin only 
} ServerAction;

typedef enum {
    CHANNEL_CREATE, // admin only
    CHANNEL_DELETE, // admin only
    CHANNEL_MESSAGE,
    CHANNEL_HISTORY_REQUEST,
} ChannelAction;

typedef enum {
    SOCIAL_FRIEND_REQUEST,
    SOCIAL_FRIEND_ACCEPT,
    SOCIAL_FRIEND_REJECT,
    SOCIAL_FRIEND_REMOVE,
    SOCIAL_LIST_FRIENDS,
    SOCIAL_LIST_USERS,
    SOCIAL_UPDATE_STATUS
} SocialAction;

// --- Payloads ---
typedef struct {
    SystemAction action;
    int status_code;
    char message[SYSTEM_MESSAGE_LEN];
} SystemPayload;

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
    int64_t message_id;
    char message[MESSAGE_LEN]; // The actual text content
} DMPayload;

typedef struct {
    ServerAction action;
    int64_t server_id;
    int64_t target_user_id; // used for actions like kick
    int status_code;
    int member_count;
    char server_name[MAX_NAME_LEN];
} ServerPayload;

typedef struct {
    ChannelAction action;
    int64_t server_id;
    int status_code;
    int64_t channel_id;
    char channel_name[MAX_NAME_LEN];
    int64_t message_id;
    char message[MESSAGE_LEN];
} ChannelPayload;

typedef struct {
    UserStatus status; 
    SocialAction action;
    int status_code;
    int64_t target_user_id; 
    char target_username[MAX_NAME_LEN]; 
    char target_status[PROFILE_STATUS_LEN + 1];
} SocialPayload;

typedef struct {
    PacketType type;
    int64_t sender_id;
    int32_t list_id;      // 0 for non-list packets; same id across one list stream
    int32_t list_index;   // 0-based item index; -1 when not applicable
    int32_t list_total;   // total items if known, otherwise -1
    ListFrameType list_frame;
    union {
        SystemPayload system;
        AuthPayload auth;
        DMPayload dm;
        ServerPayload server;
        ChannelPayload channel;
        SocialPayload social;
    } payload; 
    int64_t timestamp;
} TizcordPacket;

#endif
