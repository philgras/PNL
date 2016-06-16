/*
 * pnl_tcp_listen_test.c
 *
 *  Created on: 29.05.2016
 *      Author: philgras
 */
#include "pnl_tcp.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#define PRINT_ERROR puts(strerror(errno)); exit(EXIT_FAILURE)
#define PRINT_PNLERROR puts(pnl_strerror(rc)); exit(EXIT_FAILURE)

int main(){

	pnl_fd_t epoll;
	pnl_tcpconn_t conn;
	struct epoll_event event;
	int rc;

	epoll = epoll_create1(0);
	if(epoll == INVALID_FD){
		PRINT_ERROR;
	}


	rc = pnl_tcp_connect(&conn,"127.0.0.1","3333");
	if(rc != PNL_OK && rc != -PNL_EWAIT){
		PRINT_PNLERROR;
	}

	event.events = EPOLLET | EPOLLIN | EPOLLOUT;
	event.data.ptr = &conn;
	rc = epoll_ctl(epoll,EPOLL_CTL_ADD,conn.tcpbase.socket_fd,&event);
	if(rc < 0){
		PRINT_ERROR;
	}

	rc = epoll_wait(epoll,&event,1,-1);
	if(rc< 0){
		PRINT_ERROR;
	}

	pnl_tcpconn_t* connptr = event.data.ptr;
	int connected;
	socklen_t size= sizeof(connected);

	rc = getsockopt(connptr->tcpbase.socket_fd,SOL_SOCKET,SO_ERROR,&connected,&size);
	if(rc< 0){
			PRINT_ERROR;
	}

	printf("EPOLLIN: %d, EPOLLOUT: %d EPOLLHUP: %d, EPOLLERR: %d\n",event.events & EPOLLIN,
			event.events & EPOLLOUT,
			event.events & EPOLLHUP,
			event.events & EPOLLERR);
	printf("Success == 0 : %d", connected);

}

