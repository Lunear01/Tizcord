#include <stdio.h>
#include <stdlib.h>

#include "../include/db.h"
#include "../include/server.h"

#ifndef PORT
#define PORT 4242
#endif

int main(int argc, char* argv[]) {
    int port = PORT;
    const char* conninfo = NULL;

    if (argc >= 2) port = atoi(argv[1]);
    if (argc >= 3) conninfo = argv[2];

    ServerContext ctx;
    init_server_context(&ctx);

    if (conninfo) {
        ctx.db = db_connect(conninfo);
        if (!ctx.db) {
            fprintf(stderr, "Could not connect to database — running without persistence\n");
        }
    } else {
        printf("No DB connection string given — running without persistence.\n");
        printf("Usage: %s [port] [\"conninfo string\"]\n", argv[0]);
    }

    ctx.server_fd = start_server(port);
    run_server_loop(&ctx);

    db_disconnect(ctx.db);
    return 0;
}