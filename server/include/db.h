#ifndef DB_H
#define DB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


typedef struct DbContext DbContext;

typedef void (*MessageCallback)(int64_t msg_id, const char* username, const char* content, int64_t timestamp, void* userdata);
typedef void (*ServerCallback)(int64_t id, const char* name, int member_count, void* userdata);
typedef void (*ChannelCallback)(int64_t channel_id, const char* channel_name, void* userdata);
typedef void (*MemberCallback)(int64_t user_id, const char* username, int is_admin, void* userdata);
typedef void (*FriendCallback)(int64_t friend_id, const char* username, void* userdata);
typedef void (*FriendRequestCallback)(int64_t target_user_id, const char* username, int is_incoming, void* userdata);

/* Database connection */
DbContext* db_connect(const char* conninfo);
void db_disconnect(DbContext* db);

/* User */
int db_create_user(DbContext* db, const char* username, const char* password_hash, int64_t* user_id_out);
int db_get_password_hash(DbContext* db, const char* username, char* hash_out, int64_t* user_id_out);
int db_get_user_id_by_name(DbContext* db, const char* username, int64_t* user_id_out);

/* Server */
int db_user_is_server_admin(DbContext* db, int64_t server_id, int64_t user_id, int* is_admin_out);
int db_get_server_id(DbContext* db, const char* name, int64_t* server_id_out);
int db_create_server(DbContext* db, const char* name, int64_t user_id);
int db_join_server(DbContext* db, int64_t server_id, int64_t user_id, bool is_admin);
int db_leave_server(DbContext* db, int64_t server_id, int64_t user_id);
int db_delete_server(DbContext* db, int64_t server_id, int64_t user_id);
int db_edit_server(DbContext* db, int64_t server_id, int64_t user_id, const char* new_name);
int db_list_servers(DbContext* db, ServerCallback server_cb, void* userdata);
int db_list_server_channels(DbContext* db, int64_t server_id, ChannelCallback channel_cb, void* userdata);
int db_list_server_members(DbContext* db, int64_t server_id, MemberCallback member_cb, void* userdata);
int db_list_joined_servers(DbContext* db, int64_t user_id, ServerCallback server_cb, void* userdata);
int db_kick_server_member(DbContext* db, int64_t server_id, int64_t user_id);

/* Channel */
int db_create_channel(DbContext* db, int64_t server_id, const char* name, int64_t* channel_id_out);
int db_get_channel_id(DbContext* db, int64_t server_id, const char* name, int64_t* channel_id_out);
int db_delete_channel(DbContext* db, int64_t channel_id);
int db_list_channel_messages(DbContext* db, int64_t channel_id, MessageCallback msg_cb, void* userdata);

/* Messages */
int db_save_message(DbContext* db, int64_t channel_id, int64_t user_id, const char* content);
int db_load_history(DbContext* db, int64_t channel_id, int limit, MessageCallback msg_cb, void* userdata);
int db_edit_message(DbContext* db, int64_t message_id, const char* new_content);
int db_delete_message(DbContext* db, int64_t message_id);

/* Social */
int db_send_friend_request(DbContext* db, int64_t user_id, int64_t friend_id);
int db_accept_friend_request(DbContext* db, int64_t requester_id, int64_t accepter_id);
int db_reject_friend_request(DbContext* db, int64_t requester_id, int64_t rejecter_id);
int db_remove_friendship(DbContext* db, int64_t user_id, int64_t friend_id);
int db_list_friends(DbContext* db, int64_t user_id, FriendCallback friend_cb, void* userdata);
int db_list_friend_requests(DbContext* db, int64_t user_id, FriendRequestCallback req_cb, void* userdata);
int db_user_has_active_session(DbContext* db, int64_t user_id, int* is_online_out);

#endif