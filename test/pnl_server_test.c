/*
 * pnl_server_test.c
 *
 *  Created on: 16.06.2016
 *      Author: philgras
 */
#include "pnl_loop.h"
#include <stdio.h>
#include <string.h>
#define PNL_LIMIT 5
#define PNL_BUFFER_SIZE 1024*1
#define PNL_MSG "abcdefg"
#define NUM_OF_CONNS 10

void on_close(pnl_loop_t* l,pnl_tcp_t* tcp);
int on_accept(pnl_loop_t* l, pnl_tcpserver_t*server, pnl_tcpconn_t* conn);
int on_write(pnl_loop_t*l,pnl_tcpconn_t* conn, pnl_buffer_t* buf);
int on_read(pnl_loop_t*l,pnl_tcpconn_t* conn, pnl_buffer_t* buf);
void onstart(pnl_loop_t* l);

void onstart(pnl_loop_t* l){
	pnl_tcpserver_t* server = pnl_loop_get_data(l);
	int rc = pnl_loop_create_server(l,
													server,
													"127.0.0.1",
													"3333",
													on_close,
													on_close,
													on_accept);

	if(rc == PNL_ERR){
		pnl_loop_stop(l);
		printf("[ERROR] %s\n",pnl_strerror(server->tcpbase.error.pnl_ec));
	}
}

void on_close(pnl_loop_t* l,pnl_tcp_t* tcp){
	static unsigned int i = 0;
	void* data = pnl_tcp_get_data(tcp);

	if(data != NULL){
		free(data);
	}

	if(tcp->error.pnl_ec != PNL_NOERR){
		printf("[PNL_ERROR] %s: %p\n",pnl_strerror(tcp->error.pnl_ec),(void*)(tcp));
		printf("[ERRNO] %s: %p\n",strerror(tcp->error.system_ec),(void*)(tcp));
	}else{
		puts("Closed gracefully");
	}

	++i;

	if(i == NUM_OF_CONNS){
		pnl_loop_stop(l);
	}

}

int on_write(pnl_loop_t*l,pnl_tcpconn_t* conn, pnl_buffer_t* buf){

	unsigned int* cnt = pnl_tcp_get_data((pnl_tcp_t*)conn);

	if(*cnt == PNL_LIMIT){
		pnl_loop_stop_writing(conn);
		printf("Finished writing: %p\n",(void*)conn);
		pnl_loop_remove_connection(l,conn);
	}else{

		pnl_buffer_clear(buf);
		pnl_buffer_write(buf,PNL_MSG, strlen(PNL_MSG));
		++(*cnt);
	}

	return PNL_OK;
}

int on_read(pnl_loop_t*l,pnl_tcpconn_t* conn, pnl_buffer_t* buf){
	char str[pnl_buffer_get_used(buf)+1];
	int rc = PNL_OK;;

	strcpy(str,pnl_buffer_cbegin(buf));
	printf("Received: %s from %p\n",str, (void*)conn);

	pnl_loop_stop_reading(conn);

	unsigned int* cnt = malloc(sizeof(unsigned int));

	if(cnt != NULL){
		pnl_tcp_set_data((pnl_tcp_t*)conn, cnt);
		rc = pnl_loop_start_writing(l,conn,on_write,PNL_BUFFER_SIZE);
	}else{
		printf("Unable to allocate memory\n");
		rc = PNL_ERR;
	}

	return rc;
}

int on_accept(pnl_loop_t* l, pnl_tcpserver_t*server, pnl_tcpconn_t* conn){
	printf("Accepted: %p\n", (void*)conn);
	return pnl_loop_start_reading(l,conn, on_read,PNL_BUFFER_SIZE);
}






int main(void){

	pnl_loop_t* loop;
	pnl_tcpserver_t server;

	loop = pnl_get_platform_loop();
	pnl_loop_init(loop);
	pnl_loop_set_data(loop,&server);
	pnl_loop_start(loop,onstart);

	puts("Ende");

	return 0;
}
