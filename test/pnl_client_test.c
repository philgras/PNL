/*
 * pnl_client_test.c
 *
 *  Created on: 17.06.2016
 *      Author: philgras
 */

#include "pnl_loop.h"
#include <stdio.h>
#define PNL_LIMIT 5
#define PNL_BUFFER_SIZE 1024*1
#define PNL_MSG "abcdefg"
#define NUM_OF_CONNS 10

pnl_tcpconn_t conns[NUM_OF_CONNS];

void on_close(pnl_loop_t* l,pnl_tcp_t* tcp){

	static unsigned int i = 0;


	if(tcp->error.pnl_ec != PNL_NOERR){
		printf("[ERROR] %s\n",pnl_strerror(tcp->error.pnl_ec));
	}else{
		puts("Closed gracefully");
	}

	++i;

	if(i == NUM_OF_CONNS){
		pnl_loop_stop(l);
	}

}

int on_read(pnl_loop_t*l,pnl_tcpconn_t* conn, pnl_buffer_t* buf){

	char str[pnl_buffer_get_used(buf)+1];
	strcpy(str,pnl_buffer_cbegin(buf));
	printf("Received: %s from %p\n",str, (void*)conn);

	return PNL_OK;
}

int on_write(pnl_loop_t*l,pnl_tcpconn_t* conn, pnl_buffer_t* buf){

	void* data = pnl_tcp_get_data((pnl_tcp_t*)conn);
	int rc = PNL_OK;

	if(data != NULL){
		pnl_buffer_clear(buf);
		pnl_buffer_write(buf,PNL_MSG, strlen(PNL_MSG));
		pnl_tcp_set_data((pnl_tcp_t*)conn,NULL);
		printf("Finished writing: %p\n", (void*)conn);
	}else{
		pnl_loop_stop_writing(conn);
		rc = pnl_loop_start_reading(l,conn,on_read,PNL_BUFFER_SIZE);
	}
	return rc;
}

int on_connect(pnl_loop_t* l,pnl_tcpconn_t* conn){
	printf("Connected: %p\n", (void*)conn);
	pnl_tcp_set_data((pnl_tcp_t*)conn,PNL_MSG);
	return pnl_loop_start_writing(l,conn,on_write,PNL_BUFFER_SIZE);
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

	puts("Ende");

	return 0;
}

