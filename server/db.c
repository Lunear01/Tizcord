/* save messages to db */
#include "include/db.h"

#include <uuid/uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <stdint.h>

struct DbContext {
    sqlite3* conn;
};

static int db_err(DbContext* db, const char* ctx) {
    fprintf(stderr, "DB error [%s]: %s\n", ctx, sqlite3_errmsg(db->conn));
    return -1;
}

// Open the local SQLite database
DbContext* db_connect(const char* conninfo) {
    DbContext* db = malloc(sizeof(DbContext));
    if (!db) return NULL;
    
    if (sqlite3_open(conninfo, &db->conn) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db->conn));
        free(db);
        return NULL;
    }
    sqlite3_exec(db->conn, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    
    return db;
}

void db_disconnect(DbContext* db) {
    if (db) {
        fprintf(stderr, "Gracefully closing database connection...\n");
        sqlite3_close(db->conn);
        free(db);
    }
}

// Helper Functions
int db_user_is_server_admin(DbContext* db, int64_t server_id, int64_t user_id, int* is_admin_out) {
    const char* sql = "SELECT is_admin FROM server_members WHERE server_id = ? AND user_id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare admin check");
    }

    sqlite3_bind_int64(stmt, 1, server_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *is_admin_out = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    return -1; // Not a member or not found
}

// User Operations
int db_create_user(DbContext* db, const char* username, const char* password_hash, int64_t* user_id_out) {

    // Dummy email since email is NOT NULL UNIQUE but we skip it for now
    char email[256];
    snprintf(email, sizeof(email), "%s@dummy.com", username);

    const char* sql = "INSERT INTO users (username, email, password_hash) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare user insert");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, password_hash, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute user insert");
    }

    if (user_id_out != NULL) {
        *user_id_out = sqlite3_last_insert_rowid(db->conn);
    }

    return 0;
}

int db_get_password_hash(DbContext* db, const char* username, char* hash_out, int64_t* user_id_out) {
    const char* sql = "SELECT id, password_hash FROM users WHERE username = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare hash lookup");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    int status = -1; 

    if (rc == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const unsigned char* stored_hash = sqlite3_column_text(stmt, 1);
        if (stored_hash && hash_out) {
            strcpy(hash_out, (const char*)stored_hash);
            if (user_id_out) *user_id_out = id;
            status = 0; 
        }
    } else if (rc != SQLITE_DONE) {
        db_err(db, "Failed to execute hash lookup");
    }

    sqlite3_finalize(stmt);
    return status;
}

int db_get_user_id_by_name(DbContext* db, const char* username, int64_t* user_id_out) {
    const char* sql = "SELECT id FROM users WHERE username = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare user_id lookup");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    int status = -1;

    if (rc == SQLITE_ROW) {
        if (user_id_out) {
            *user_id_out = sqlite3_column_int64(stmt, 0);
        }
        status = 0;
    }

    sqlite3_finalize(stmt);
    return status;
}

int db_save_message(DbContext* db, int64_t channel_id, int64_t user_id, const char* content) {
    const char* sql = "INSERT INTO messages (channel_id, user_id, content) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare message insert");
    }

    // Bind the 64-bit integers directly
    sqlite3_bind_int64(stmt, 1, channel_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute message insert");
    }

    return 0; 
}

// Direct message operations
int db_save_direct_message(DbContext* db, int64_t sender_id, int64_t receiver_id, const char* content) {
    const char* sql = "INSERT INTO direct_messages (sender_id, receiver_id, content) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_int64(stmt, 1, sender_id);
    sqlite3_bind_int64(stmt, 2, receiver_id);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_list_direct_messages(DbContext* db, int64_t sender_id, int64_t receiver_id, DirectMessageCallback dm_cb, void* userdata) {
    // Double JOIN to get both the sender's username and receiver's username
    const char* sql = 
        "SELECT * FROM ("
        "  SELECT dm.id, u_sender.username, u_receiver.username, dm.content, dm.created_at, dm.sender_id "
        "  FROM direct_messages dm "
        "  JOIN users u_sender ON dm.sender_id = u_sender.id "
        "  JOIN users u_receiver ON dm.receiver_id = u_receiver.id "
        "  WHERE (dm.sender_id = ? AND dm.receiver_id = ?) "
        "     OR (dm.sender_id = ? AND dm.receiver_id = ?) "
        "  ORDER BY dm.id DESC LIMIT 50"
        ") ORDER BY id ASC;";
        
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare direct message history query");
    }
    
    sqlite3_bind_int64(stmt, 1, sender_id);
    sqlite3_bind_int64(stmt, 2, receiver_id);
    sqlite3_bind_int64(stmt, 3, receiver_id);
    sqlite3_bind_int64(stmt, 4, sender_id);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t msg_id = sqlite3_column_int64(stmt, 0);
        const char* sender_name = (const char*)sqlite3_column_text(stmt, 1);
        const char* receiver_name = (const char*)sqlite3_column_text(stmt, 2);
        const char* content = (const char*)sqlite3_column_text(stmt, 3);
        
        int64_t timestamp = time(NULL);
        if (sqlite3_column_count(stmt) > 4 && sqlite3_column_type(stmt, 4) == SQLITE_INTEGER) {
            timestamp = sqlite3_column_int64(stmt, 4);
        }
        
        if (dm_cb) {
            dm_cb(msg_id, 
                  sender_name ? sender_name : "Unknown", 
                  receiver_name ? receiver_name : "Unknown", 
                  content ? content : "", 
                  timestamp, 
                  userdata);
        }
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

// Packet server operations
int db_get_server(DbContext* db, const char* name, int64_t* server_id_out){
    const char* sql_select = "SELECT id FROM servers WHERE name = ?;";
    sqlite3_stmt* stmt_select;
    
    if (sqlite3_prepare_v2(db->conn, sql_select, -1, &stmt_select, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare server select");
    }

    sqlite3_bind_text(stmt_select, 1, name, strlen(name), SQLITE_STATIC);

    int rc = sqlite3_step(stmt_select);
    if (rc == SQLITE_ROW) {
        *server_id_out = sqlite3_column_int64(stmt_select, 0);
        sqlite3_finalize(stmt_select);
        return 0;
    }
    sqlite3_finalize(stmt_select);
    return -1; // Not found
}

int db_create_server(DbContext* db, const char* name, int64_t user_id) {
    const char* sql_insert = "INSERT INTO servers (name) VALUES (?);";
    sqlite3_stmt* stmt_insert;
    
    if (sqlite3_prepare_v2(db->conn, sql_insert, -1, &stmt_insert, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare server insert");
    }

    sqlite3_bind_text(stmt_insert, 1, name, strlen(name), SQLITE_STATIC);

    int rc = sqlite3_step(stmt_insert);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt_insert);
        return db_err(db, "Failed to execute server insert");
    }
    sqlite3_finalize(stmt_insert);

    int64_t server_id_out = sqlite3_last_insert_rowid(db->conn);

    // Add creator as admin member of the server
    return db_join_server(db, server_id_out, user_id, true);
}

int db_join_server(DbContext* db, int64_t server_id, int64_t user_id, bool is_admin){
    const char* sql = "INSERT INTO server_members (server_id, user_id, is_admin) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare server join");
    }

    sqlite3_bind_int64(stmt, 1, server_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, is_admin);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute server join");
    }

    return 0;
}

int db_leave_server(DbContext* db, int64_t server_id, int64_t user_id){
    const char* sql = "DELETE FROM server_members WHERE server_id = ? AND user_id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare server leave");
    }

    sqlite3_bind_int64(stmt, 1, server_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute server leave");
    }

    return 0;
}

int db_delete_server(DbContext* db, int64_t server_id, int64_t user_id){
    if (db_user_is_server_admin(db, server_id, user_id, &(int){0}) == -1) {
        return -1; // User is not a member or not found
    }

    const char* sql = "DELETE FROM servers WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare server delete");
    }

    sqlite3_bind_int64(stmt, 1, server_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute server delete");
    }

    return 0;
}

int db_edit_server(DbContext* db, int64_t server_id, int64_t user_id, const char* new_name){
    if (db_user_is_server_admin(db, server_id, user_id, &(int){0}) == -1) {
        return -1; // User is not a member or not found
    }

    const char* sql = "UPDATE servers SET name = ? WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare server edit");
    }

    sqlite3_bind_text(stmt, 1, new_name, strlen(new_name), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, server_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute server edit");
    }

    return 0;
}

int db_list_servers(DbContext* db, ServerCallback server_cb, void *userdata) {
    // Join with server_members to count how many users are in each server
    const char* sql = 
        "SELECT s.id, s.name, COUNT(sm.user_id) "
        "FROM servers s "
        "LEFT JOIN server_members sm ON s.id = sm.server_id "
        "GROUP BY s.id "
        "ORDER BY s.name ASC;";
    
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (server_cb != NULL) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char* name = (const char*)sqlite3_column_text(stmt, 1);
            int count = sqlite3_column_int(stmt, 2); // Get the counted members
            server_cb(id, name != NULL ? name : "", count, userdata);
        }
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_list_server_channels(DbContext* db, int64_t server_id, ChannelCallback channel_cb, void* userdata) {
    const char* sql =
        "SELECT id, name "
        "FROM channels "
        "WHERE server_id = ? "
        "ORDER BY name ASC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare channel list");
    }

    sqlite3_bind_int64(stmt, 1, server_id);

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (channel_cb != NULL) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char* name = (const char*)sqlite3_column_text(stmt, 1);
            channel_cb(id, name != NULL ? name : "", userdata);
        }
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed while iterating channel list");
    }

    return 0;
}

int db_list_server_members(DbContext* db, int64_t server_id, MemberCallback member_cb, void* userdata) {
    const char* sql =
        "SELECT u.id, u.username, sm.is_admin "
        "FROM server_members sm "
        "JOIN users u ON u.id = sm.user_id "
        "WHERE sm.server_id = ? "
        "ORDER BY u.username ASC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare member list");
    }

    sqlite3_bind_int64(stmt, 1, server_id);

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (member_cb != NULL) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char* username = (const char*)sqlite3_column_text(stmt, 1);
            int is_admin = sqlite3_column_int(stmt, 2);
            member_cb(id, username != NULL ? username : "", is_admin, userdata);
        }
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed while iterating member list");
    }

    return 0;
}

int db_list_joined_servers(DbContext* db, int64_t user_id, ServerCallback server_cb, void* userdata) {
    // Use a subquery to count the members for each server the user is in
    const char* sql =
        "SELECT s.id, s.name, "
        "  (SELECT COUNT(*) FROM server_members WHERE server_id = s.id) as count "
        "FROM server_members sm "
        "JOIN servers s ON s.id = sm.server_id "
        "WHERE sm.user_id = ? "
        "ORDER BY s.name ASC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare joined server list");
    }

    sqlite3_bind_int64(stmt, 1, user_id);

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (server_cb != NULL) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char* name = (const char*)sqlite3_column_text(stmt, 1);
            int count = sqlite3_column_int(stmt, 2); // Get the counted members
            server_cb(id, name != NULL ? name : "", count, userdata);
        }
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed while iterating joined server list");
    }

    return 0;
}

int db_kick_server_member(DbContext* db, int64_t server_id, int64_t user_id) {
    if (db_user_is_server_admin(db, server_id, user_id, &(int){0}) == -1) {
        return -1; // User is not a member or not found
    }

    const char* sql = "DELETE FROM server_members WHERE server_id = ? AND user_id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare kick member");
    }

    sqlite3_bind_int64(stmt, 1, server_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute kick member");
    }

    return sqlite3_changes(db->conn) > 0 ? 0 : -1;
}

// Packet social operations
int db_send_friend_request(DbContext* db, int64_t user_id, int64_t friend_id) {
    int same_direction_status = -1;
    int reciprocal_status = -1;
    const char* existing_sql =
        "SELECT user_id, friend_id, is_accepted "
        "FROM friends "
        "WHERE (user_id = ? AND friend_id = ?) "
        "   OR (user_id = ? AND friend_id = ?);";
    sqlite3_stmt* existing_stmt;

    if (sqlite3_prepare_v2(db->conn, existing_sql, -1, &existing_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(existing_stmt, 1, user_id);
        sqlite3_bind_int64(existing_stmt, 2, friend_id);
        sqlite3_bind_int64(existing_stmt, 3, friend_id);
        sqlite3_bind_int64(existing_stmt, 4, user_id);

        while (sqlite3_step(existing_stmt) == SQLITE_ROW) {
            int64_t existing_user_id = sqlite3_column_int64(existing_stmt, 0);
            int64_t existing_friend_id = sqlite3_column_int64(existing_stmt, 1);
            int is_accepted = sqlite3_column_int(existing_stmt, 2);

            if (existing_user_id == user_id && existing_friend_id == friend_id) {
                same_direction_status = is_accepted;
            } else if (existing_user_id == friend_id && existing_friend_id == user_id) {
                reciprocal_status = is_accepted;
            }
        }
        sqlite3_finalize(existing_stmt);
    }

    if (same_direction_status == 1 || reciprocal_status == 1) {
        return -2;
    }
    if (reciprocal_status == 0) {
        return db_accept_friend_request(db, user_id, friend_id) == 0 ? 1 : -1;
    }

    const char* sql = "INSERT INTO friends (user_id, friend_id, is_accepted) VALUES (?, ?, 0);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare friend request");
    }

    sqlite3_bind_int64(stmt, 1, user_id);
    sqlite3_bind_int64(stmt, 2, friend_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute friend request");
    }

    return 0;
}

int db_accept_friend_request(DbContext* db, int64_t my_user_id, int64_t friend_id) {
    int is_accepted = -1;
    const char* check_sql = "SELECT is_accepted FROM friends WHERE user_id = ? AND friend_id = ?;";
    sqlite3_stmt* check_stmt;
    if (sqlite3_prepare_v2(db->conn, check_sql, -1, &check_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(check_stmt, 1, friend_id);
        sqlite3_bind_int64(check_stmt, 2, my_user_id);
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            is_accepted = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);
    }
    
    if (is_accepted == -1) {
        return -1; // Not found
    }
    if (is_accepted == 1) {
        return -2; // Already friends
    }
    
    // Accept the request
    const char* up_sql = "UPDATE friends SET is_accepted = 1 WHERE user_id = ? AND friend_id = ?;";
    sqlite3_stmt* up_stmt;
    if (sqlite3_prepare_v2(db->conn, up_sql, -1, &up_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(up_stmt, 1, friend_id);
        sqlite3_bind_int64(up_stmt, 2, my_user_id);
        sqlite3_step(up_stmt);
        sqlite3_finalize(up_stmt);
    }
    
    // Delete reciprocal request
    const char* del_sql = "DELETE FROM friends WHERE user_id = ? AND friend_id = ? AND is_accepted = 0;";
    sqlite3_stmt* del_stmt;
    if (sqlite3_prepare_v2(db->conn, del_sql, -1, &del_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(del_stmt, 1, my_user_id);
        sqlite3_bind_int64(del_stmt, 2, friend_id);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);
    }

    return 0;
}

int db_remove_friendship(DbContext* db, int64_t user_id, int64_t friend_id) {
    // Check if they are actually friends (is_accepted = 1) in either direction
    const char* check_sql =
        "SELECT id FROM friends WHERE is_accepted = 1 AND "
        "((user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?));";
    sqlite3_stmt* check_stmt;
    int found = 0;

    if (sqlite3_prepare_v2(db->conn, check_sql, -1, &check_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(check_stmt, 1, user_id);
        sqlite3_bind_int64(check_stmt, 2, friend_id);
        sqlite3_bind_int64(check_stmt, 3, friend_id);
        sqlite3_bind_int64(check_stmt, 4, user_id);
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            found = 1;
        }
        sqlite3_finalize(check_stmt);
    }

    if (!found) return -1; // Not friends

    const char* del_sql =
        "DELETE FROM friends WHERE "
        "((user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?));";
    sqlite3_stmt* del_stmt;
    if (sqlite3_prepare_v2(db->conn, del_sql, -1, &del_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(del_stmt, 1, user_id);
        sqlite3_bind_int64(del_stmt, 2, friend_id);
        sqlite3_bind_int64(del_stmt, 3, friend_id);
        sqlite3_bind_int64(del_stmt, 4, user_id);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);
    }

    return 0;
}

int db_reject_friend_request(DbContext* db, int64_t my_user_id, int64_t sender_id) {
    // Only delete if there's a pending incoming request (sender sent it to me)
    const char* check_sql = "SELECT id FROM friends WHERE user_id = ? AND friend_id = ? AND is_accepted = 0;";
    sqlite3_stmt* check_stmt;
    int found = 0;

    if (sqlite3_prepare_v2(db->conn, check_sql, -1, &check_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(check_stmt, 1, sender_id);
        sqlite3_bind_int64(check_stmt, 2, my_user_id);
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            found = 1;
        }
        sqlite3_finalize(check_stmt);
    }

    if (!found) return -1; // No incoming request from this user

    const char* del_sql = "DELETE FROM friends WHERE user_id = ? AND friend_id = ? AND is_accepted = 0;";
    sqlite3_stmt* del_stmt;
    if (sqlite3_prepare_v2(db->conn, del_sql, -1, &del_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(del_stmt, 1, sender_id);
        sqlite3_bind_int64(del_stmt, 2, my_user_id);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);
    }

    return 0;
}

int db_list_friend_requests(DbContext* db, int64_t user_id, FriendRequestCallback req_cb, void* userdata) {
    if (!req_cb) return -1;

    const char* sql = 
        "SELECT u.id, u.username, "
        "  CASE WHEN f.is_accepted = 1 THEN 2 ELSE 1 END AS is_incoming "
        "FROM friends f JOIN users u ON f.user_id = u.id "
        "WHERE f.friend_id = ? "
        "UNION ALL "
        "SELECT u.id, u.username, "
        "  CASE WHEN f.is_accepted = 1 THEN 2 ELSE 0 END AS is_incoming "
        "FROM friends f JOIN users u ON f.friend_id = u.id "
        "WHERE f.user_id = ?;";

    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare friend requests select");
    }

    sqlite3_bind_int64(stmt, 1, user_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int64_t target_id = sqlite3_column_int64(stmt, 0);
        const char* username = (const char*)sqlite3_column_text(stmt, 1);
        int is_incoming = sqlite3_column_int(stmt, 2);
        
        req_cb(target_id, username ? username : "", is_incoming, userdata);
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed while iterating friend requests");
    }

    return 0;
}

int db_list_channel_messages(DbContext* db, int64_t channel_id, MessageCallback msg_cb, void* userdata) {
    // We use a subquery to get the 50 MOST RECENT messages, but order them chronologically
    const char* sql = 
        "SELECT * FROM ("
        "  SELECT m.id, u.username, m.content, m.created_at "
        "  FROM messages m "
        "  JOIN users u ON m.user_id = u.id "
        "  WHERE m.channel_id = ? "
        "  ORDER BY m.created_at DESC LIMIT 50"
        ") ORDER BY created_at ASC;";
        
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, channel_id);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t msg_id = sqlite3_column_int64(stmt, 0);
        const char* username = (const char*)sqlite3_column_text(stmt, 1);
        const char* content = (const char*)sqlite3_column_text(stmt, 2);
        int64_t timestamp = sqlite3_column_int64(stmt, 3);
        
        if (msg_cb) msg_cb(msg_id, username ? username : "Unknown", content ? content : "", timestamp, userdata);
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

int db_create_channel(DbContext* db, int64_t server_id, const char* name, int64_t* channel_id_out) {
    const char* sql = "INSERT INTO channels (server_id, name) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, server_id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        if (channel_id_out != NULL) {
            *channel_id_out = sqlite3_last_insert_rowid(db->conn);
        }
        return 0;
    }
    
    return -1;
}

int db_get_channel_id(DbContext* db, int64_t server_id, const char* name, int64_t* channel_id_out) {
    const char* sql = "SELECT id FROM channels WHERE server_id = ? AND name = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, server_id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW && channel_id_out != NULL) {
        *channel_id_out = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW) ? 0 : -1;
}

int db_delete_channel(DbContext* db, int64_t channel_id) {
    const char* sql = "DELETE FROM channels WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, channel_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_channel_server_id(DbContext* db, int64_t channel_id, int64_t* server_id_out) {
    const char* sql = "SELECT server_id FROM channels WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, channel_id);
    
    int rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW && server_id_out != NULL) {
        *server_id_out = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW) ? 0 : -1;
}

int db_list_all_users(DbContext* db, UserListCallback user_cb, void* userdata) {
    if (!user_cb) return -1;

    const char* sql = "SELECT id, username, status FROM users ORDER BY username ASC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int64_t user_id = sqlite3_column_int64(stmt, 0);
        const char* username = (const char*)sqlite3_column_text(stmt, 1);
        const char* status = (const char*)sqlite3_column_text(stmt, 2);
        user_cb(user_id, username ? username : "", status ? status : "", userdata);
    }

    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_update_user_status(DbContext* db, int64_t user_id, const char* status) {
    const char* sql = "UPDATE users SET status = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (db == NULL || status == NULL) {
        return -1;
    }

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare user status update");
    }

    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute user status update");
    }

    return sqlite3_changes(db->conn) > 0 ? 0 : -1;
}
