#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shared/tls.h"
#include "include/db.h"
#include "include/server.h"

#ifndef PORT
#define PORT 4242
#endif

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <port> <db> (<cert.pem> <key.pem> | --insecure)\n", prog);
    fprintf(stderr, "  --insecure runs plaintext TCP; only for local benchmarks.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    if (argc != 4 && argc != 5) usage(argv[0]);
    int port = atoi(argv[1]);
    const char* conninfo = argv[2];

    if (argc == 5) {
        if (tls_init_server(argv[3], argv[4]) != 0) {
            fprintf(stderr, "Could not load TLS certificate/key\n");
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(argv[3], "--insecure") == 0) {
        fprintf(stderr, "WARNING: TLS disabled, traffic is plaintext\n");
    } else {
        usage(argv[0]);
    }

    DbContext* db = db_connect(conninfo);
    if (!db) {
        fprintf(stderr, "Could not connect to database\n");
        exit(EXIT_FAILURE);
    }

    ServerContext ctx;
    init_server_context(&ctx, db);

    ctx.server_fd = start_server(port);
    run_server_loop(&ctx);

    tls_cleanup();
    db_disconnect(ctx.db);
    fprintf(stderr, "Server shutting down.\n");
    return 0;
}
