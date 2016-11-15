/*
 * pnl_syscalls.h
 *
 *  Created on: 29.07.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_SYS_NIO_H_
#define SRC_PNL_SYS_NIO_H_

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>


extern int pnl_getaddrinfo(const char *__restrict name, const char *__restrict service,
                           const struct addrinfo *__restrict req, struct addrinfo **__restrict pai);

extern void pnl_freeaddrinfo(struct addrinfo *ai);

extern int pnl_socket(int domain, int type, int protocol);

extern int pnl_connect(int fd, const struct sockaddr *addr, socklen_t len);

extern int pnl_bind(int fd, const struct sockaddr *addr, socklen_t len);

extern int pnl_close(int fd);

extern int pnl_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);

extern int pnl_getsockopt(int fd, int level, int optname, void *__restrict optval, socklen_t *__restrict optlen);

extern int pnl_listen(int fd, int n);

extern int pnl_accept4(int fd, struct sockaddr *addr, socklen_t *__restrict addr_len, int flags);

extern ssize_t pnl_recv(int fd, void *buf, size_t n, int flags);

extern ssize_t pnl_send(int fd, const void *buf, size_t n, int flags);

#endif /* SRC_PNL_SYS_NIO_H_ */
