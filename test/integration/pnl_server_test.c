/*
 * pnl_server_test.c
 *
 *  Created on: 16.06.2016
 *      Author: philgras
 */
#include <stdio.h>
#include <string.h>

#include "../src/pnl.h"
#define PNL_BUFFER_SIZE 1024*1
#define PNL_MSG "abc"
#define NUM_OF_CONNS 10

void on_close(pnl_loop_t* l,pnl_base_t* tcp);
int on_accept(pnl_loop_t* l, pnl_server_t*server, pnl_connection_t* conn);
int on_write(pnl_loop_t*l,pnl_connection_t* conn, pnl_buffer_t* buf);
int on_read(pnl_loop_t*l,pnl_connection_t* conn, pnl_buffer_t* buf);
void onstart(pnl_loop_t* l);

void onstart(pnl_loop_t* l){
	pnl_server_t* server = pnl_loop_get_data(l);
	int rc = pnl_loop_add_server(l,
													server,
													"127.0.0.1",
													"3333",
													on_close,
													on_close,
													on_accept);

	if(rc == PNL_ERR){
		const pnl_error_t* err = pnl_base_get_error((pnl_base_t*)server);
		pnl_loop_stop(l);
		printf("[ERROR] %s\n",pnl_strerror(err->pnl_ec));
	}
}

void on_close(pnl_loop_t* l,pnl_base_t* tcp){
	static unsigned int i = 0;
	void* data = pnl_base_get_data(tcp);

	if(data != NULL){
		free(data);
	}

	if(tcp->error.pnl_ec != PNL_NOERR){
		const pnl_error_t* err = pnl_base_get_error(tcp);
		printf("[PNL_ERROR] %s: %p\n",pnl_strerror(err->pnl_ec),(void*)(tcp));
		printf("[ERRNO] %s: %p\n",strerror(err->system_ec),(void*)(tcp));
	}else{
		puts("Closed gracefully");
	}

	++i;

	if(i == NUM_OF_CONNS){
		pnl_loop_stop(l);
	}

}

int on_write(pnl_loop_t*l,pnl_connection_t* conn, pnl_buffer_t* buf){

	char * ptr = pnl_base_get_data((pnl_base_t*)conn);

	if(ptr == 0x0){
		pnl_loop_stop_writing(conn);
		printf("Finished writing: %p\n",(void*)conn);
		pnl_loop_remove_connection(l,conn);
	}else{

		pnl_buffer_clear(buf);
		pnl_buffer_write(buf,PNL_MSG, strlen(PNL_MSG));
		pnl_base_set_data((pnl_base_t*)conn,--ptr);
		printf("Connection %p has to write %p more times\n", (void*) conn, (void*) ptr);
	}

	return PNL_OK;
}

int on_read(pnl_loop_t*l,pnl_connection_t* conn, pnl_buffer_t* buf){
	size_t size = pnl_buffer_get_used(buf)+1;
	char str[size];
	int rc = PNL_OK;

	memcpy(str,pnl_buffer_cbegin(buf), size);
	str[size]  = '\0';
	printf("Received: %s from %p\n",str, (void*)conn);

	if(strchr(str,'#')){
		pnl_loop_stop_reading(conn);
		printf("Finished reading from %p\n", (void*)conn);
		pnl_base_set_data((pnl_base_t*)conn, (void*)0x3);
		rc = pnl_loop_start_writing(l,conn,on_write,PNL_BUFFER_SIZE);
	}

	return rc;
}

int on_accept(pnl_loop_t* l, pnl_server_t*server, pnl_connection_t* conn){
	printf("Accepted: %p\n", (void*)conn);
	return pnl_loop_start_reading(l,conn, on_read,PNL_BUFFER_SIZE);
}






int main(void){

	pnl_loop_t* loop;
	pnl_server_t server;

	loop = pnl_get_platform_loop();
	pnl_loop_init(loop);
	pnl_loop_set_data(loop,&server);
	pnl_loop_start(loop,onstart);

	puts("Ende");

	return 0;
}
