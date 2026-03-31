#ifndef CLIENT_HELPER_H

#define CLIENT_HELPER_H
#include "protocol.h"

int send_packet_to_client(int client_fd, const TizcordPacket *packet);
int send_action_response(int client_fd, PacketType type, int action, int status_code,
						 const char *message);
int send_list_item_response(int client_fd, const TizcordPacket *item_packet);
int send_list_response(int client_fd, PacketType type, int action,
                       const char *const *items, const int64_t *item_ids,
                       const int *item_counts, size_t item_count);
int send_list_end_response(int client_fd, PacketType type);
int send_message_response(int client_fd, const char *username, const char *content,
						  const char *timestamp);

#endif