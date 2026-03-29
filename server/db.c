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

/* -------------- HELPER FUNCTIONS ------------------ */
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

/* -------------- USER OPERATIONS ------------------ */
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

int db_save_message(DbContext* db, int64_t channel_id, sqlite3_int64 user_id, const char* content) {
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

int db_edit_message(DbContext* db, int64_t message_id, const char* new_content) {
    const char* sql = "UPDATE messages SET content = ? WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare message update");
    }

    sqlite3_bind_text(stmt, 1, new_content, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, message_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute message update");
    }

    return 0;
}

int db_delete_message(DbContext* db, int64_t message_id) {
    const char* sql = "DELETE FROM messages WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare message delete");
    }

    sqlite3_bind_int64(stmt, 1, message_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute message delete");
    }

    return 0;
}

/* -------------- SERVER OPERATIONS ------------------ */
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

int db_create_server(DbContext* db, const char* name, int64_t user_id, int64_t* server_id_out) {
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

    *server_id_out = sqlite3_last_insert_rowid(db->conn);

    // Add creator as admin member of the server
    return db_join_server(db, *server_id_out, user_id, true);
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

int db_list_servers(DbContext* db, ServerCallback server_cb, void* userdata) {
    const char* sql = "SELECT id, name FROM servers ORDER BY name ASC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare server list");
    }

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (server_cb != NULL) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char* name = (const char*)sqlite3_column_text(stmt, 1);
            server_cb(id, name != NULL ? name : "", userdata);
        }
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed while iterating server list");
    }

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
    const char* sql =
        "SELECT s.id, s.name "
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
            server_cb(id, name != NULL ? name : "", userdata);
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