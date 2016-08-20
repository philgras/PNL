/*
 * pnl_tcp.h
 *
 *  Created on: 19.07.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_TCP_H_
#define SRC_PNL_TCP_H_

#include "pnl_common.h"

extern int pnl_tcp_connect(pnl_fd_t*conn_fd, const char* ip, const char* port, pnl_error_t* error);

extern int pnl_tcp_connect_succeeded(pnl_fd_t* conn_fd, pnl_error_t* error );

extern int pnl_tcp_listen(pnl_fd_t* server_fd, const char* ip, const char* port, pnl_error_t* error);

extern int pnl_tcp_accept(pnl_fd_t* server_fd, pnl_fd_t* conn_fd, pnl_error_t* error);

extern int pnl_tcp_close(pnl_fd_t* fd, pnl_error_t* error);

extern int pnl_tcp_read(pnl_fd_t* conn_fd, pnl_buffer_t* buffer, pnl_error_t* error);

extern int pnl_tcp_write(pnl_fd_t* conn_fd, pnl_buffer_t* buffer, pnl_error_t* error);

#endif /* SRC_PNL_TCP_H_ */

