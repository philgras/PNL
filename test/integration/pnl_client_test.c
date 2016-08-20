/*
 * pnl_client_test.c
 *
 *  Created on: 17.06.2016
 *      Author: philgras
 */

#include <stdio.h>

#include "../src/pnl.h"
#define PNL_LIMIT 5
#define PNL_BUFFER_SIZE 1024*1
#define PNL_MSG "abcdefg#"
#define NUM_OF_CONNS 10

pnl_connection_t conns[NUM_OF_CONNS];

void on_close(pnl_loop_t* l,pnl_base_t* tcp){

	static unsigned int i = 0;


	if(tcp->error.pnl_ec != PNL_NOERR){
		printf("[ERROR] %s\n",pnl_strerror(tcp->error.pnl_ec));
		printf("[ERRNO] %s: %p\n",strerror(tcp->error.system_ec),(void*)(tcp));
	}else{
		puts("Closed gracefully");
	}

	++i;

	if(i == NUM_OF_CONNS){
		pnl_loop_stop(l);
	}

}

int on_read(pnl_loop_t*l,pnl_connection_t* conn, pnl_buffer_t* buf){

	char str[pnl_buffer_get_used(buf)+1];
	strcpy(str,pnl_buffer_cbegin(buf));
	printf("Received: %s from %p\n",str, (void*)conn);


	return PNL_OK;
}

int on_write(pnl_loop_t*l,pnl_connection_t* conn, pnl_buffer_t* buf){

	void* data = pnl_base_get_data((pnl_base_t*)conn);
	int rc = PNL_OK;

	if(data != NULL){
		pnl_buffer_clear(buf);
		pnl_buffer_write(buf,PNL_MSG, strlen(PNL_MSG));
		pnl_base_set_data((pnl_base_t*)conn,NULL);
	}else{
		pnl_loop_stop_writing(conn);
		printf("Finished writing: %p\n", (void*)conn);
		rc = pnl_loop_start_reading(l,conn,on_read,PNL_BUFFER_SIZE);
	}
	return rc;
}

int on_connect(pnl_loop_t* l,pnl_connection_t* conn){
	printf("Connected: %p\n", (void*)conn);
	pnl_base_set_data((pnl_base_t*)conn,PNL_MSG);
	return pnl_loop_start_writing(l,conn,on_write,PNL_BUFFER_SIZE);
}

void onstart(pnl_loop_t* l){

	for(int i = 0; i < sizeof(conns)/sizeof(pnl_connection_t); ++i){
	int rc = pnl_loop_add_connection(l,
													&conns[i],
													"127.0.0.1",
													"3333",
													on_close,
													on_connect);

		if(rc == PNL_ERR){
			const pnl_error_t* err = pnl_base_get_error((pnl_base_t*)&conns[i]);
			pnl_loop_stop(l);
			printf("[ERROR] %s\n",pnl_strerror(err->pnl_ec));
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

