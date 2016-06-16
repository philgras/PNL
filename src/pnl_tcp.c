/*
 * pnl_tcp.c
 *
 *  Created on: 27.05.2016
 *      Author: philgras
 */

#include "pnl_tcp.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>


#define WHILE_EINTR(func, rc) 							\
				do{ 														\
					rc = func; 											\
				}while( rc == -1&& errno == EINTR)

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

int pnl_tcp_connect(pnl_tcpconn_t* conn , const char* ip, const char* port){


	int rc;
	pnl_fd_t sock;
	struct addrinfo hints, *res;

	assert(conn->tcpbase.socket_fd != INVALID_FD);

	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	rc = getaddrinfo(ip,port,&hints,&res);
	if(rc != 0){
		/*
		 *  The system_ec is set to the getaddrinfo error code which means
		 * one should use gai_strerror call to get the error string.
		 */
		PNL_TCP_ERROR(&conn->tcpbase, PNL_EADDRINFO , rc);
		return PNL_ERR;
	}

	for(struct addrinfo* iter = res; iter != NULL; iter = iter->ai_next){

		/*acquire socket*/
		sock = socket(iter->ai_family,iter->ai_socktype | SOCK_NONBLOCK, iter->ai_protocol);
		if (sock == INVALID_FD) {
			PNL_TCP_ERROR(&conn->tcpbase, PNL_ESOCKET , errno);
			continue;
		}

		/*connect*/
	    WHILE_EINTR(connect(sock, iter->ai_addr, iter->ai_addrlen),rc);
		if(rc == -1){
			if(errno == EINPROGRESS){
				PNL_TCP_ERROR(&conn->tcpbase, PNL_EWAIT, 0);
			}else{
				PNL_TCP_ERROR(&conn->tcpbase, PNL_ECONNECT, errno);
				close(sock);
				sock = INVALID_FD;
				continue;
			}
		}else{
			/*if another iteration step has set the error codes*/
			PNL_TCP_ERROR(&conn->tcpbase, 0, 0);
		}

		break;
	}

	freeaddrinfo(res);

	if(sock != INVALID_FD ){
		conn->tcpbase.socket_fd = sock;

		if(conn->tcpbase.error.pnl_ec == 0) rc = PNL_OK;
		else rc = PNL_ERR;

	}else{
		rc = PNL_ERR;
	}

	return rc;
}

int pnl_tcp_connect_succeeded(pnl_tcpconn_t* conn ){

	int connected;
	int rc = PNL_OK;
	socklen_t size= sizeof(connected);
	if(getsockopt(conn->tcpbase.socket_fd,SOL_SOCKET,SO_ERROR,&connected,&size) < 0){
		PNL_TCP_ERROR(&conn->tcpbase,PNL_EGETSOCKOPT,errno);
		rc = PNL_ERR;
	}else if(connected != 0){
		PNL_TCP_ERROR(&conn->tcpbase,PNL_ECONNECT,connected);
		rc = PNL_ERR;
	}
	return rc;
}

int pnl_tcp_listen(pnl_tcpserver_t* server, const char* ip, const char* port){

	int rc;
	int yes = 1;
	pnl_fd_t sock;
	struct addrinfo * res, hints;

	assert(server->tcpbase.socket_fd != INVALID_FD);

	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	rc = getaddrinfo(ip,port,&hints,&res);
	if(rc != 0){
		/*
		 *  The system_ec is set to the getaddrinfo error code which means
		 * one should use gai_strerror call to get the error string.
		 */
		PNL_TCP_ERROR(&server->tcpbase, PNL_EADDRINFO , rc);
		return PNL_ERR;
	}

	for(struct addrinfo* iter = res; iter != NULL; iter = iter->ai_next){

		sock = socket(iter->ai_family,iter->ai_socktype | SOCK_NONBLOCK, iter->ai_protocol);
		if (sock == INVALID_FD) {
			PNL_TCP_ERROR(&server->tcpbase,PNL_ESOCKET,errno);
			continue;
		}

		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))	== -1) {
			PNL_TCP_ERROR(&server->tcpbase,PNL_ESETSOCKOPT,errno);
			close(sock);
			sock = INVALID_FD;
			break;
		}

		if (bind(sock, iter->ai_addr, iter->ai_addrlen) == -1) {
			PNL_TCP_ERROR(&server->tcpbase,PNL_EBIND,errno);
			close(sock);
			sock = INVALID_FD;
			continue;
		}

		PNL_TCP_ERROR(&server->tcpbase,0,0);
		break;
	}

	freeaddrinfo(res);

	if(sock != INVALID_FD ){
		if(listen(sock , SOMAXCONN) != 0){
			PNL_TCP_ERROR(&server->tcpbase,PNL_ELISTEN,errno);
			close(sock);
			rc = PNL_ERR;
		}else{
			server->tcpbase.socket_fd = sock;
			rc = PNL_OK;
		}
	}else{
		rc = PNL_ERR;;
	}

	return rc;
}

int pnl_tcp_accept(pnl_tcpserver_t* server, pnl_tcpconn_t* conn){

	pnl_fd_t socket;
	int rc;

	do{
		socket = accept4(server->tcpbase.socket_fd,NULL,NULL,SOCK_NONBLOCK);

		if(socket == INVALID_FD){
			if(errno == ECONNABORTED || errno == EINTR){
				continue;
			}else if (errno == EWOULDBLOCK || errno == EAGAIN){
				PNL_TCP_ERROR(&server->tcpbase,PNL_EWAIT,errno);
				rc = PNL_ERR;
				break;
			}else{
				/*
				 * TODO more accurate error handling
				 * --> distinguish between connection and server errors
				 */
				PNL_TCP_ERROR(&server->tcpbase,PNL_EACCEPT,errno);
				rc = PNL_ERR;
				break;
			}
		}else{
			conn->tcpbase.socket_fd = socket;
			rc = PNL_OK;
			break;
		}
	}while(1);

	return rc;
}

int pnl_tcp_close(pnl_tcp_t*t){
	/* this invalidates the socket fd in any case*/
	int rc = close(t->socket_fd);
	t->socket_fd= INVALID_FD;

	if(rc < 0){
		PNL_TCP_CLEANUP_ERROR(t,PNL_ECLOSE,errno);
		rc = PNL_ERR;
	}else{
		rc = PNL_OK;
	}

	return rc;
}

int pnl_tcp_read(pnl_tcpconn_t* conn){

	int rc;

	conn->rbuffer->used = 0;

	WHILE_EINTR(recv(conn->tcpbase.socket_fd,
									   conn->rbuffer->data,
									   conn->rbuffer->length,
									   0),rc);

	if(rc > 0){
		conn->rbuffer->used += rc;
		rc = PNL_OK;
	}else if(rc == 0){
		rc = PNL_ERR;
		PNL_TCP_ERROR(&conn->tcpbase,PNL_EPEERCLO,0);
	}else{
		rc = PNL_ERR;
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			PNL_TCP_ERROR(&conn->tcpbase,PNL_EWAIT,errno);
		}else{
			PNL_TCP_ERROR(&conn->tcpbase,PNL_ERECV,errno);
		}
	}

	conn->rbuffer->last_position = conn->rbuffer->data;
	return rc;

}

int pnl_tcp_write(pnl_tcpconn_t* conn){

	int rc;

	while(conn->wbuffer->used > 0){

		WHILE_EINTR(send(conn->tcpbase.socket_fd,
								   conn->wbuffer->last_position,
								   conn->wbuffer->used,
								   0),rc);

		if(rc > 0){
			conn->wbuffer->last_position += rc;
			conn->wbuffer->used -= rc;
			rc = PNL_OK;
			continue;

		}else{
			rc = PNL_ERR;
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				PNL_TCP_ERROR(&conn->tcpbase,PNL_EWAIT,errno);
			}else{
				PNL_TCP_ERROR(&conn->tcpbase,PNL_ERECV,errno);
			}
			break;
		}
	}

	if(conn->wbuffer->used == 0){
		conn->wbuffer->last_position = conn->wbuffer->data;
	}
	return rc;
}


