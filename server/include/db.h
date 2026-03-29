#ifndef DB_H
#define DB_H

#include <sqlite3.h>
typedef struct DbContext DbContext;

/* Database connection */
DbContext* db_connect(const char* conninfo);
void db_disconnect(DbContext* db);

/* User */
int db_create_user(DbContext* db, const char* username, const char* password_hash, sqlite3_int64* user_id_out);
int db_get_password_hash(DbContext* db, const char* username, char* hash_out, sqlite3_int64* user_id_out);

/* Server */
int db_get_or_create_server(DbContext* db, const char* name, sqlite3_int64* server_id_out);
int db_join_server(DbContext* db, sqlite3_int64 server_id, sqlite3_int64 user_id);
int db_leave_server(DbContext* db, sqlite3_int64 server_id, sqlite3_int64 user_id);

/* Channel */
int db_get_or_create_channel(DbContext* db, sqlite3_int64 server_id, const char* name, sqlite3_int64* channel_id_out);
int db_delete_channel(DbContext* db, sqlite3_int64 channel_id);

/* Messages */
int db_save_message(DbContext* db, sqlite3_int64 channel_id, sqlite3_int64 user_id, const char* content);
typedef void (*MessageCallback)(const char* username, const char* content, const char* timestamp, void* userdata);

int db_load_history(DbContext* db, sqlite3_int64 channel_id, int limit, MessageCallback msg_cb, void* userdata);

int db_edit_message(DbContext* db, sqlite3_int64 message_id, const char* new_content);
int db_delete_message(DbContext* db, sqlite3_int64 message_id);

#endif