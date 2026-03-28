/* Channel operations */
#include "server.h"

void list_channels(ServerContext* ctx, const char* name);
void join_channel(ServerContext* ctx, int client_index, const char* channel_name);
void delete_channel(ServerContext* ctx, const char* channel_id);
void send_channel_message(ServerContext* ctx, int client_index, const char* channel_id,
                          const char* msg, int len);
void edit_channel_message(ServerContext* ctx, int client_index, const char* channel_id,
                          const char* message_id, const char* new_msg, int len);
void delete_channel_message(ServerContext* ctx, int client_index, const char* channel_id,
                            const char* message_id);