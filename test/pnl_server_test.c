/*
 * pnl_server_test.c
 *
 *  Created on: 16.06.2016
 *      Author: philgras
 */
#include "pnl_loop.h"
#include <stdio.h>

pnl_tcpserver_t server;
void on_close(pnl_loop_t* l,pnl_tcp_t* tcp){

	if(tcp->error.pnl_ec != PNL_NOERR){
		printf("[ERROR] %s\n",pnl_strerror(tcp->error.pnl_ec));
	}else{
		puts("Closed gracefully");
	}

}

void on_start(pnl_loop_t* l){
	int rc = pnl_loop_create_server(l,
													&server,
													"127.0.0.1",
													"3333",
													on_close,
													on_close,
													on_accept);

	if(rc == PNL_ERR){
		pnl_loop_stop(l);
		printf("[ERROR] %s\n",pnl_strerror(server.tcpbase->error.pnl_ec));
	}
}

int on_accept(pnl_loop_t* l, pnl_tcpserver_t*server, pnl_tcpconn_t* conn){
	return PNL_OK;
}



int main(void){

	pnl_loop_t* loop;

	loop = pnl_get_platform_loop();
	pnl_loop_init(loop);

	pnl_loop_start(loop,on_start);

	return 0;
}
