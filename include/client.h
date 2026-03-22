typedef struct {
    uint64_t id;
    char status[512];
    char username[64];
    char* passwordHash;
} User;

typedef struct {
    uint64_t id;
    char *channel_name;
    char **users;
    char *
} Channel;
