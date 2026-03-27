#include <stdio.h>
#include <stdlib.h>

#include "include/db.h"
#include "include/server.h"

#ifndef PORT
#define PORT 4242
#endif

int main(int argc, char* argv[]) {
    int port = PORT;
    const char* conninfo = NULL;

    if (argc >= 2) port = atoi(argv[1]);
    if (argc >= 3) conninfo = argv[2];

    DbContext* db = NULL;
    if (conninfo) {
        db = db_connect(conninfo);
        if (!db) {
            fprintf(stderr, "Could not connect to database\n");
            exit(EXIT_FAILURE);
        }
    } 
    else {
        printf("No DB connection string given.\n");
        printf("Usage: %s [port] [\"conninfo string\"]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ServerContext ctx;
    init_server_context(&ctx);

    ctx.server_fd = start_server(port);
    run_server_loop(&ctx);

    db_disconnect(ctx.db);
    fprinf(stderr, "Server shutting down.\n");
    return 0;
}