/* save messages to db */
#include "../include/db.h"

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct DbContext {
    PGconn* conn;
};

static int db_err(DbContext* db, const char* ctx) {
    fprintf(stderr, "DB error [%s]: %s\n", ctx, PQerrorMessage(db->conn));
    return -1;
}
