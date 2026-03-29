/* save messages to db */
#include "include/db.h"

#include <sqlite3.h>
#include <uuid/uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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

int db_create_user(DbContext* db, const char* username, const char* password_hash, sqlite3_int64* user_id_out) {

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

int db_get_password_hash(DbContext* db, const char* username, char* hash_out, sqlite3_int64* user_id_out) {
    const char* sql = "SELECT id, password_hash FROM users WHERE username = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare hash lookup");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    int status = -1; 

    if (rc == SQLITE_ROW) {
        sqlite3_int64 id = sqlite3_column_int64(stmt, 0);
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

int db_save_message(DbContext* db, sqlite3_int64 channel_id, sqlite3_int64 user_id, const char* content) {
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

int db_edit_message(DbContext* db, sqlite3_int64 message_id, const char* new_content) {
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

int db_delete_message(DbContext* db, sqlite3_int64 message_id) {
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

