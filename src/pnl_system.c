/*
 * pnl_sys_nio.c
 *
 *  Created on: 29.07.2016
 *      Author: philgras
 */
#include "pnl_system.h"

int pnl_getaddrinfo(const char *__restrict name, const char *__restrict service,
                    const struct addrinfo *__restrict req, struct addrinfo **__restrict pai) {
    return getaddrinfo(name, service, req, pai);
}


void pnl_freeaddrinfo(struct addrinfo *ai) {
    freeaddrinfo(ai);
}

int pnl_socket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int pnl_connect(int fd, const struct sockaddr *addr, socklen_t len) {
    return connect(fd, addr, len);
}

int pnl_bind(int fd, const struct sockaddr *addr, socklen_t len) {
    return bind(fd, addr, len);
}

int pnl_close(int fd) {
    return close(fd);
}

int pnl_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) {
    return setsockopt(fd, level, optname, optval, optlen);
}

int pnl_getsockopt(int fd, int level, int optname, void *__restrict optval, socklen_t *__restrict optlen) {
    return getsockopt(fd, level, optname, optval, optlen);
}

int pnl_listen(int fd, int n) {
    return listen(fd, n);
}

int pnl_accept4(int fd, struct sockaddr *addr, socklen_t *__restrict addr_len, int flags) {
    return accept4(fd, addr, addr_len, flags);
}

ssize_t pnl_recv(int fd, void *buf, size_t n, int flags) {
    return recv(fd, buf, n, flags);
}

ssize_t pnl_send(int fd, const void *buf, size_t n, int flags) {
    return send(fd, buf, n, flags);
}

