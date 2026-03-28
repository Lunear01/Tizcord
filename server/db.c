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
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    
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

