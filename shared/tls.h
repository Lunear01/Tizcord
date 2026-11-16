#ifndef TLS_H
#define TLS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/* TLS over an existing TCP fd. One SSL_CTX per process; one SSL per fd.
 * When no context is initialised, tls_send/tls_recv fall back to plain
 * send/recv, so callers never branch on TLS. */

int tls_init_server(const char *cert_file, const char *key_file);
int tls_init_client(const char *ca_file);
bool tls_enabled(void);
void tls_cleanup(void);

/* Server handshake, driven from a select loop so a silent peer blocks no one.
 * start sets fd O_NONBLOCK; step returns 1 done, 0 awaiting peer, -1 failed.
 * On -1 the caller still tls_close()s. */
int tls_accept_start(int fd);
int tls_accept_step(int fd);

/* Blocking client handshake; verifies the certificate's IP SAN. */
int tls_connect(int fd, const char *ip);

/* Shutdown and free the SSL for fd. Caller still close()s the fd. */
void tls_close(int fd);

/* Bytes already decrypted and buffered inside OpenSSL for fd. A select()
 * loop must drain these before blocking again. */
int tls_pending(int fd);

/* send/recv equivalents. Return like the syscalls; errno is set on -1.
 * nonblock makes tls_recv return -1/EAGAIN instead of waiting. */
ssize_t tls_send(int fd, const void *buf, size_t len);
ssize_t tls_recv(int fd, void *buf, size_t len, bool nonblock);

#endif
