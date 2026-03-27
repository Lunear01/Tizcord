/* save messages to db */
#include "../include/db.h"

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

// Helper function to generate a 36-character UUID string
void generate_uuid_string(char* out_str) {
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    uuid_unparse(binuuid, out_str);
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
    
    // Turn on Foreign Keys
    sqlite3_exec(db->conn, "PRAGMA foreign_keys = ON;", 0, 0, 0);

    return db;
}

void db_disconnect(DbContext* db) {
    if (db) {
        sqlite3_close(db->conn);
        free(db);
    }
}

int db_create_user(DbContext* db, const char* username, const char* password_hash, char* user_id_out) {
    // Generate UUID
    char new_id[37];
    generate_uuid_string(new_id);

    // Dummy email since email is NOT NULL UNIQUE but we skip it for now
    char email[256];
    snprintf(email, sizeof(email), "%s@dummy.com", username);

    const char* sql = "INSERT INTO users (id, username, email, password_hash) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return db_err(db, "Failed to prepare user insert");
    }

    sqlite3_bind_text(stmt, 1, new_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, password_hash, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return db_err(db, "Failed to execute user insert");
    }

    if (user_id_out) {
        strcpy(user_id_out, new_id);
    }
    return 0;
}

//TODO: Implement:
// db_edit_message
// db_delete_message 
int db_edit_message(DbContext* db, const char* message_id, const char* new_content){
    //TODO
}
int db_delete_message(DbContext* db, const char* message_id){
    //TODO
}