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
#include <signal.h>

#define PRINT_ERROR puts(strerror(errno)); exit(EXIT_FAILURE)
#define PRINT_PNLERROR puts(pnl_strerror(rc)); exit(EXIT_FAILURE)

volatile sig_atomic_t running = 1;

void handler(int sig){

	running = 0;
	signal(sig,SIG_DFL);

}

int main(){


	pnl_tcpserver_t server;
	pnl_tcpconn_t conn;

	int rc;

	signal(SIGINT,handler);

	rc = pnl_tcp_listen(&server,NULL,"3333");
	if(rc != PNL_OK){
		PRINT_PNLERROR;
	}


	puts("Listening");

	while(running){
		rc = pnl_tcp_accept(&server,&conn);
		puts("Got a connection");

	}

	puts("closing connection");
	pnl_tcp_close(&(conn.tcpbase));

}

