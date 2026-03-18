// This is the file that both server and client will include
// Prevents segfaults when packets being sent/received 
typedef enum {
    MSG_LOGIN,
    MSG_CHAT,
    MSG_JOIN_CHANNEL,
    MSG_EDIT_POST,
    MSG_POLL_CREATE
} PacketType;

typedef struct {
    PacketType type;
    char sender[32];
    char content[512];
    long timestamp;
} TizcordPacket;