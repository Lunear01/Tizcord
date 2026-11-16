#include "tls.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

static SSL_CTX *ctx = NULL;
static SSL *ssl_by_fd[FD_SETSIZE];

static void log_ssl_errors(const char *where) {
    unsigned long e;
    while ((e = ERR_get_error()) != 0) {
        fprintf(stderr, "[TLS] %s: %s\n", where, ERR_reason_error_string(e));
    }
}

static SSL_CTX *new_context(const SSL_METHOD *method) {
    SSL_CTX *c = SSL_CTX_new(method);
    if (c == NULL) {
        log_ssl_errors("SSL_CTX_new");
        return NULL;
    }
    if (SSL_CTX_set_min_proto_version(c, TLS1_3_VERSION) != 1) {
        log_ssl_errors("SSL_CTX_set_min_proto_version");
        SSL_CTX_free(c);
        return NULL;
    }
    /* Partial writes let tls_send retry with an advanced pointer. */
    SSL_CTX_set_mode(c, SSL_MODE_ENABLE_PARTIAL_WRITE |
                            SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER |
                            SSL_MODE_AUTO_RETRY);
    /* A peer that closes without close_notify is a plain EOF here; the
     * fixed-size packet framing makes truncation detectable anyway. */
    SSL_CTX_set_options(c, SSL_OP_IGNORE_UNEXPECTED_EOF);
    /* SSL_write cannot pass MSG_NOSIGNAL. */
    signal(SIGPIPE, SIG_IGN);
    return c;
}

int tls_init_server(const char *cert_file, const char *key_file) {
    ctx = new_context(TLS_server_method());
    if (ctx == NULL) return -1;
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) != 1 ||
        SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(ctx) != 1) {
        log_ssl_errors("loading certificate/key");
        SSL_CTX_free(ctx);
        ctx = NULL;
        return -1;
    }
    return 0;
}

int tls_init_client(const char *ca_file) {
    ctx = new_context(TLS_client_method());
    if (ctx == NULL) return -1;
    if (SSL_CTX_load_verify_locations(ctx, ca_file, NULL) != 1) {
        log_ssl_errors("loading CA certificate");
        SSL_CTX_free(ctx);
        ctx = NULL;
        return -1;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    return 0;
}

bool tls_enabled(void) { return ctx != NULL; }

void tls_cleanup(void) {
    for (int fd = 0; fd < FD_SETSIZE; fd++) tls_close(fd);
    if (ctx != NULL) SSL_CTX_free(ctx);
    ctx = NULL;
}

static SSL *ssl_for(int fd) {
    if (fd < 0 || fd >= FD_SETSIZE) return NULL;
    return ssl_by_fd[fd];
}

static SSL *new_ssl(int fd) {
    if (ctx == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (fd < 0 || fd >= FD_SETSIZE) {
        errno = EBADF;
        return NULL;
    }
    SSL *ssl = SSL_new(ctx);
    if (ssl == NULL || SSL_set_fd(ssl, fd) != 1) {
        log_ssl_errors("SSL_new");
        if (ssl != NULL) SSL_free(ssl);
        return NULL;
    }
    return ssl;
}

int tls_accept_start(int fd) {
    SSL *ssl = new_ssl(fd);
    if (ssl == NULL) return -1;

    /* Server reads poll from a select loop; SSL_read has no MSG_DONTWAIT. */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        SSL_free(ssl);
        return -1;
    }
    SSL_set_accept_state(ssl);
    ssl_by_fd[fd] = ssl;
    return 0;
}

int tls_accept_step(int fd) {
    SSL *ssl = ssl_for(fd);
    if (ssl == NULL) return -1;
    int r = SSL_do_handshake(ssl);
    if (r == 1) return 1;
    int err = SSL_get_error(ssl, r);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
    log_ssl_errors("SSL_accept");
    return -1;
}

int tls_connect(int fd, const char *ip) {
    SSL *ssl = new_ssl(fd);
    if (ssl == NULL) return -1;

    /* Peer is addressed by IP, so match the certificate's IP SAN. */
    if (X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(ssl), ip) != 1) {
        log_ssl_errors("set verify IP");
        SSL_free(ssl);
        return -1;
    }

    if (SSL_connect(ssl) != 1) {
        log_ssl_errors("SSL_connect");
        long verify = SSL_get_verify_result(ssl);
        if (verify != X509_V_OK) {
            fprintf(stderr, "[TLS] certificate rejected: %s\n",
                    X509_verify_cert_error_string(verify));
        }
        SSL_free(ssl);
        return -1;
    }
    ssl_by_fd[fd] = ssl;
    return 0;
}

void tls_close(int fd) {
    SSL *ssl = ssl_for(fd);
    if (ssl == NULL) return;
    SSL_shutdown(ssl);
    SSL_free(ssl);
    ssl_by_fd[fd] = NULL;
}

int tls_pending(int fd) {
    SSL *ssl = ssl_for(fd);
    return ssl == NULL ? 0 : SSL_pending(ssl);
}

/* Block until fd is readable or writable. */
static void wait_fd(int fd, bool writable) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    select(fd + 1, writable ? NULL : &set, writable ? &set : NULL, NULL, NULL);
}

/* Translate an SSL_get_error code into errno. Returns 0 for clean EOF. */
static ssize_t map_error(int err) {
    switch (err) {
    case SSL_ERROR_ZERO_RETURN:
        return 0;
    case SSL_ERROR_SYSCALL:
        if (errno == 0) errno = ECONNRESET;
        return -1;
    default:
        log_ssl_errors("SSL I/O");
        errno = EPROTO;
        return -1;
    }
}

ssize_t tls_send(int fd, const void *buf, size_t len) {
    SSL *ssl = ssl_for(fd);
    if (ssl == NULL) return send(fd, buf, len, MSG_NOSIGNAL);

    for (;;) {
        int n = SSL_write(ssl, buf, (int)len);
        if (n > 0) return n;
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
            /* Keep the blocking-write contract on a non-blocking fd. */
            wait_fd(fd, err == SSL_ERROR_WANT_WRITE);
            continue;
        }
        ssize_t r = map_error(err);
        if (r == 0) errno = EPIPE;
        return -1;
    }
}

ssize_t tls_recv(int fd, void *buf, size_t len, bool nonblock) {
    SSL *ssl = ssl_for(fd);
    if (ssl == NULL) return recv(fd, buf, len, nonblock ? MSG_DONTWAIT : 0);

    for (;;) {
        int n = SSL_read(ssl, buf, (int)len);
        if (n > 0) return n;
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            if (nonblock) {
                errno = EAGAIN;
                return -1;
            }
            wait_fd(fd, err == SSL_ERROR_WANT_WRITE);
            continue;
        }
        return map_error(err);
    }
}
