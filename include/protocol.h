// This is the file that both server and client will include
// Prevents segfaults when packets being sent/received 
typedef enum {
    MSG_LOGIN,
    MSG_DM,
    MSG_SERVER,
    MSG_CHANNEL,
    MSG_BROADCAST // For Testing
} PacketType;

typedef struct {

} AuthPayload;


typedef struct {

} DMPayload;

typedef struct {

} ServerPayload;

typedef struct {

} ChannelPayload;

typedef struct {
    PacketType type;
    char sender[32];
    union {
        AuthPayload auth;
        DMPayload dm;
        ServerPayload server;
        ChannelPayload channel;
    }
    long timestamp;
    char receiver[32];
} TizcordPacket;
