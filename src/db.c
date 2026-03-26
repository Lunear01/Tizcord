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
