#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <stddef.h>

typedef struct DbContext DbContext;

typedef void (*MessageCallback)(const char* username, const char* content, const char* timestamp, void* userdata);
typedef void (*ServerCallback)(sqlite3_int64 server_id, const char* server_name, void* userdata);
typedef void (*ChannelCallback)(sqlite3_int64 channel_id, const char* channel_name, void* userdata);
typedef void (*MemberCallback)(sqlite3_int64 user_id, const char* username, int is_admin, void* userdata);
typedef void (*FriendCallback)(sqlite3_int64 friend_id, const char* username, void* userdata);

/* Database connection */
DbContext* db_connect(const char* conninfo);
void db_disconnect(DbContext* db);

/* User */
int db_create_user(DbContext* db, const char* username, const char* password_hash, sqlite3_int64* user_id_out);
int db_get_user_credentials(DbContext* db, const char* username, sqlite3_int64* user_id_out,
							char* password_hash_out, size_t password_hash_out_len);

/* Server */
int db_get_or_create_server(DbContext* db, const char* name, sqlite3_int64* server_id_out);
int db_join_server(DbContext* db, sqlite3_int64 server_id, sqlite3_int64 user_id);
int db_leave_server(DbContext* db, sqlite3_int64 server_id, sqlite3_int64 user_id);
int db_delete_server(DbContext* db, sqlite3_int64 server_id);
int db_edit_server(DbContext* db, sqlite3_int64 server_id, const char* new_name);
int db_list_servers(DbContext* db, ServerCallback server_cb, void* userdata);
int db_list_server_channels(DbContext* db, sqlite3_int64 server_id, ChannelCallback channel_cb, void* userdata);
int db_list_server_members(DbContext* db, sqlite3_int64 server_id, MemberCallback member_cb, void* userdata);
int db_list_joined_servers(DbContext* db, sqlite3_int64 user_id, ServerCallback server_cb, void* userdata);
int db_kick_server_member(DbContext* db, sqlite3_int64 server_id, sqlite3_int64 user_id);

/* Channel */
int db_get_or_create_channel(DbContext* db, sqlite3_int64 server_id, const char* name, sqlite3_int64* channel_id_out);
int db_get_channel_id(DbContext* db, sqlite3_int64 server_id, const char* name, sqlite3_int64* channel_id_out);
int db_delete_channel(DbContext* db, sqlite3_int64 channel_id);

/* Messages */
int db_save_message(DbContext* db, sqlite3_int64 channel_id, sqlite3_int64 user_id, const char* content);
int db_load_history(DbContext* db, sqlite3_int64 channel_id, int limit, MessageCallback msg_cb, void* userdata);
int db_edit_message(DbContext* db, sqlite3_int64 message_id, const char* new_content);
int db_delete_message(DbContext* db, sqlite3_int64 message_id);

/* Social */
int db_send_friend_request(DbContext* db, sqlite3_int64 user_id, sqlite3_int64 friend_id);
int db_accept_friend_request(DbContext* db, sqlite3_int64 requester_id, sqlite3_int64 accepter_id);
int db_reject_friend_request(DbContext* db, sqlite3_int64 requester_id, sqlite3_int64 rejecter_id);
int db_remove_friendship(DbContext* db, sqlite3_int64 user_id, sqlite3_int64 friend_id);
int db_list_friends(DbContext* db, sqlite3_int64 user_id, FriendCallback friend_cb, void* userdata);
int db_user_has_active_session(DbContext* db, sqlite3_int64 user_id, int* is_online_out);

#endif