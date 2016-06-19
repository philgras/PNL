/*
 * pnl_client_test.c
 *
 *  Created on: 17.06.2016
 *      Author: philgras
 */

#include "pnl_loop.h"
#include <stdio.h>

pnl_tcpconn_t conns[10];

void on_close(pnl_loop_t* l,pnl_tcp_t* tcp){

	printf("FD: %d\n",tcp->socket_fd);

	if(tcp->error.pnl_ec != PNL_NOERR){
		printf("[ERROR] %s\n",pnl_strerror(tcp->error.pnl_ec));
	}else{
		puts("Closed gracefully");
	}

}

int on_connect(pnl_loop_t* l,pnl_tcpconn_t* conn){
	puts("connected");
	return PNL_OK;
}

void onstart(pnl_loop_t* l){

	for(int i = 0; i < sizeof(conns)/sizeof(pnl_tcpconn_t); ++i){
	int rc = pnl_loop_create_connection(l,
													&conns[i],
													"127.0.0.1",
													"3333",
													on_close,
													on_connect);

		if(rc == PNL_ERR){
			pnl_loop_stop(l);
			printf("[ERROR] %s\n",pnl_strerror(conns[i].tcpbase.error.pnl_ec));
		}

	}
}





int main(void){

	pnl_loop_t* loop;

	loop = pnl_get_platform_loop();
	pnl_loop_init(loop);

	pnl_loop_start(loop,onstart);

	return 0;
}

