
typedef struct DbContext DbContext;

/* Helper Utilities */
void generate_uuid_string(char* out_str);

/* Database connection */
DbContext* db_connect(const char* conninfo);
void db_disconnect(DbContext* db);

/* User */
int db_create_user(DbContext* db, const char* username, const char* password_hash, char* user_id_out);

/* Server */
int db_get_or_create_server(DbContext* db, const char* name, char* server_id_out, int out_len);
int db_join_server(DbContext* db, const char* server_id, const char* user_id);
int db_leave_server(DbContext* db, const char* server_id, const char* user_id);

/* Channel */
int db_get_or_create_channel(DbContext* db, const char* server_id, const char* name,
                             char* channel_id_out, int out_len);
int db_delete_channel(DbContext* db, const char* channel_id);

/* Messages */
int db_save_message(DbContext* db, const char* channel_id, const char* user_id,
                    const char* content);
typedef void (*MessageCallback)(const char* username, const char* content, const char* timestamp,
                                void* userdata);

int db_load_history(DbContext* db, const char* channel_id, int limit, MessageCallback msg_cb,
                    void* userdata);

int db_edit_message(DbContext* db, const char* message_id, const char* new_content);
int db_delete_message(DbContext* db, const char* message_id);