#ifndef CLIENT_H
#define CLIENT_H

void connect_to_server(const char *ip_address, int port);

int send_register(const char *username, const char *password);
int send_login(const char *username, const char *password);

void leave_server(int server_id);

void send_friend_request(const char *target_username);
void accept_friend_request(const char *target_username);
void list_joined_servers_request(void);

#endif