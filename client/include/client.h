#ifndef CLIENT_H
#define CLIENT_H

// Connections
void connect_to_server(const char *ip_address, int port);

// Auth
int send_register(const char *username, const char *password);
int send_login(const char *username, const char *password);

// Tizcord Server
void create_server(const char *server_name);
void delete_server(int64_t server_id);
void leave_server(int server_id);
void list_all_servers_request(void);
void join_server(int64_t server_id);
void kick_server_member(int64_t server_id, int64_t target_user_id);

// Tizcord Channel
void create_channel(int server_id, const char *channel_name);
void delete_channel(int64_t channel_id);
void send_channel_message(int64_t channel_id, const char *message);

// Tizcord Social
void send_friend_request(const char *target_username);
void accept_friend_request(const char *target_username);
void unfriend(const char *target_username);
void reject_friend_request(const char *target_username);
void send_status_update(const char *status_text);
void request_friend_list(void);
void request_user_list(void);
void list_joined_servers_request(void);

// Helpers
void request_server_channels(int64_t server_id);
void request_server_members(int64_t server_id);
void request_channel_history(int64_t channel_id);

// Direct Message
void send_dm_message(int64_t recipient_id, const char *message);
void request_dm_history(int64_t friend_id);

#endif
