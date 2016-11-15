/*
 * pnl_tcp.c
 *
 *  Created on: 27.05.2016
 *      Author: philgras
 */
#include "pnl_system.h"
#include "pnl_tcp.h"

#include <errno.h>
#include <assert.h>
#include <string.h>


#define WHILE_EINTR(func, rc)                            \
                do{                                      \
                    rc = func;                           \
                }while( rc == -1&& errno == EINTR)


/*
    static int enable_nonblocking(pnl_tcp_t* tcp_handle){
	int flags;

	flags = fcntl(tcp_handle->socket_fd, F_GETFL, 0);
	if (flags == -1) {
		PNL_TCP_ERROR(tcp_handle,PNL_ENONBLOCK,errno);
		return PNL_ERR;
	}

	flags |= O_NONBLOCK;
	if (fcntl(tcp_handle->socket_fd, F_SETFL, flags) == -1) {
		PNL_TCP_ERROR(tcp_handle,PNL_ENONBLOCK,errno);
		return PNL_ERR;
	}

	return PNL_OK;
}
*/



int pnl_tcp_connect(pnl_fd_t *conn_fd, const char *ip, const char *port, pnl_error_t *error) {


    int rc;
    pnl_fd_t sock = PNL_INVALID_FD;
    struct addrinfo hints, *res;

    assert(*conn_fd == PNL_INVALID_FD);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    rc = pnl_getaddrinfo(ip, port, &hints, &res);
    if (rc != 0) {

        /*
         *  The system_ec is set to the getaddrinfo error code which means
         * one should use gai_strerror call to get the error string.
         */

        pnl_error_set(error, PNL_EADDRINFO, rc);
        return PNL_ERR;
    }

    for (struct addrinfo *iter = res; iter != NULL; iter = iter->ai_next) {

        /*acquire socket*/
        sock = pnl_socket(iter->ai_family, iter->ai_socktype | SOCK_NONBLOCK, iter->ai_protocol);
        if (sock == PNL_INVALID_FD) {
            pnl_error_set(error, PNL_ESOCKET, errno);
            continue;
        }

        /*connect*/
        WHILE_EINTR(pnl_connect(sock, iter->ai_addr, iter->ai_addrlen), rc);
        if (rc == -1) {
            if (errno == EINPROGRESS) {
                pnl_error_set(error, PNL_EWAIT, 0);
            } else {
                pnl_error_set(error, PNL_ECONNECT, errno);
                pnl_close(sock);
                sock = PNL_INVALID_FD;
                continue;
            }
        } else {
            /*if another iteration step has set the error codes*/
            pnl_error_set(error, 0, 0);
        }

        break;
    }

    pnl_freeaddrinfo(res);

    if (sock != PNL_INVALID_FD) {
        *conn_fd = sock;

        if (error->pnl_ec == 0) rc = PNL_OK;
        else rc = PNL_ERR;

    } else {
        rc = PNL_ERR;
    }

    return rc;
}


int pnl_tcp_connect_succeeded(pnl_fd_t *conn_fd, pnl_error_t *error) {

    int connected;
    int rc = PNL_OK;
    socklen_t size = sizeof(connected);

    if (pnl_getsockopt(*conn_fd, SOL_SOCKET, SO_ERROR, &connected, &size) < 0) {

        pnl_error_set(error, PNL_EGETSOCKOPT, errno);
        rc = PNL_ERR;

    } else if (connected != 0) {

        if (connected == EINPROGRESS)  {
            pnl_error_set(error, PNL_EWAIT, connected);
        }else{
            pnl_error_set(error, PNL_ECONNECT, connected);
        }

        rc = PNL_ERR;
    }
    return rc;
}

int pnl_tcp_listen(pnl_fd_t *server_fd, const char *ip, const char *port, pnl_error_t *error) {

    int rc;
    int yes = 1;
    pnl_fd_t sock = PNL_INVALID_FD;
    struct addrinfo *res, hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    rc = pnl_getaddrinfo(ip, port, &hints, &res);
    if (rc != 0) {
        /*
         *  The system_ec is set to the getaddrinfo error code which means
         * one should use gai_strerror call to get the error string.
         */
        pnl_error_set(error, PNL_EADDRINFO, rc);
        return PNL_ERR;
    }

    for (struct addrinfo *iter = res; iter != NULL; iter = iter->ai_next) {

        sock = pnl_socket(iter->ai_family, iter->ai_socktype | SOCK_NONBLOCK, iter->ai_protocol);
        if (sock == PNL_INVALID_FD) {
            pnl_error_set(error, PNL_ESOCKET, errno);
            continue;
        }

        if (pnl_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            pnl_error_set(error, PNL_ESETSOCKOPT, errno);
            pnl_close(sock);
            sock = PNL_INVALID_FD;
            break;
        }

        if (pnl_bind(sock, iter->ai_addr, iter->ai_addrlen) == -1) {
            pnl_error_set(error, PNL_EBIND, errno);
            pnl_close(sock);
            sock = PNL_INVALID_FD;
            continue;
        }

        pnl_error_set(error, 0, 0);
        break;
    }

    pnl_freeaddrinfo(res);

    if (sock != PNL_INVALID_FD) {
        if (pnl_listen(sock, SOMAXCONN) != 0) {
            pnl_error_set(error, PNL_ELISTEN, errno);
            pnl_close(sock);
            rc = PNL_ERR;
        } else {
            *server_fd = sock;
            rc = PNL_OK;
        }
    } else {
        rc = PNL_ERR;;
    }

    return rc;
}

int pnl_tcp_accept(pnl_fd_t *server_fd, pnl_fd_t *conn_fd, pnl_error_t *error) {

    pnl_fd_t socket;
    int rc;

    do {
        socket = pnl_accept4(*server_fd, NULL, NULL, SOCK_NONBLOCK);

        if (socket == PNL_INVALID_FD) {
            if (errno == ECONNABORTED || errno == EINTR) {
                continue;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                pnl_error_set(error, PNL_EWAIT, errno);
                rc = PNL_ERR;
                break;
            } else {

                pnl_error_set(error, PNL_EACCEPT, errno);
                rc = PNL_ERR;
                break;
            }
        } else {
            *conn_fd = socket;
            rc = PNL_OK;
            break;
        }
    } while (1);

    return rc;
}

int pnl_tcp_close(pnl_fd_t *fd, pnl_error_t *error) {

    /* this invalidates the socket fd in any case*/
    int rc = pnl_close(*fd);
    *fd = PNL_INVALID_FD;

    if (rc < 0) {
        pnl_error_set_cleanup(error, PNL_ECLOSE, errno);
        rc = PNL_ERR;
    } else {
        rc = PNL_OK;
    }

    return rc;
}

int pnl_tcp_read(pnl_fd_t *conn_fd, pnl_buffer_t *buffer, pnl_error_t *error) {

    int rc;
    char *position = pnl_buffer_used_end(buffer);

    WHILE_EINTR(pnl_recv(*conn_fd,
                         position,
                         pnl_buffer_get_space(buffer),
                         0), rc);

    if (rc > 0) {
        pnl_buffer_add_used(buffer, (size_t) rc);
        pnl_buffer_set_position(buffer, position + rc);

        rc = PNL_OK;
    } else if (rc == 0) {
        rc = PNL_ERR;
        pnl_error_set(error, PNL_EPEERCLO, 0);
    } else {
        rc = PNL_ERR;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            pnl_error_set(error, PNL_EWAIT, errno);
        } else {
            pnl_error_set(error, PNL_ERECV, errno);
        }
    }

    return rc;

}

int pnl_tcp_write(pnl_fd_t *conn_fd, pnl_buffer_t *buffer, pnl_error_t *error) {

    int rc = PNL_OK;
    char *position = pnl_buffer_get_position(buffer);
    char *end_of_used = pnl_buffer_used_end(buffer);
    while (end_of_used - position > 0) {

        WHILE_EINTR(pnl_send(*conn_fd,
                             position,
                             end_of_used - position,
                             0), rc);

        if (rc > 0) {
            position += rc;
            rc = PNL_OK;
            continue;

        } else {
            rc = PNL_ERR;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                pnl_error_set(error, PNL_EWAIT, errno);
            } else {
                pnl_error_set(error, PNL_ESEND, errno);
            }
            break;
        }
    }

    pnl_buffer_set_position(buffer, position);

    return rc;
}


