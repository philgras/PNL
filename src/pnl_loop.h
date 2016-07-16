#ifndef PNL_LOOP_H
#define PNL_LOOP_H

#include "pnl_common.h"
#include "pnl_list.h"
#include "pnl_tcp.h"

#include <inttypes.h>



typedef void (*on_start)(pnl_loop_t* l);

/*
 * LOOP
 */
struct pnl_loop_s{
    pnl_fd_t epollfd;

    pnl_list_t active_tcp;
    pnl_list_t inactive_tcp;
    
    pnl_time_t system_time;
    pnl_time_t poll_timeout;

    void* data;

    volatile unsigned int running:1;

};




pnl_loop_t* pnl_get_platform_loop(void);

void pnl_loop_init(pnl_loop_t*);

int pnl_loop_create_server(pnl_loop_t* l,
										  pnl_tcpserver_t* server,
										  const char* ip,
										  const char* port,
										  on_close_cb on_server_close,
										  on_close_cb on_conn_close,
										  on_accept_cb on_accept);

int pnl_loop_remove_server(pnl_loop_t* l, pnl_tcpserver_t*);

int pnl_loop_create_connection(pnl_loop_t* l,
							   pnl_tcpconn_t* conn,
							   const char* ip,
							   const char* port,
							   on_close_cb on_close,
							   on_connect_cb on_connect);

int pnl_loop_remove_connection(pnl_loop_t* l, pnl_tcpconn_t* conn);

int pnl_loop_start_reading(pnl_loop_t* l, pnl_tcpconn_t* conn, on_ioevent_cb on_read,size_t buffer_size);
void  pnl_loop_stop_reading(pnl_tcpconn_t* conn);
int pnl_loop_start_writing(pnl_loop_t* l, pnl_tcpconn_t* conn, on_ioevent_cb on_write, size_t buffer_size);
void pnl_loop_stop_writing(pnl_tcpconn_t* conn);

int pnl_loop_start(pnl_loop_t*, on_start onstart);
void pnl_loop_stop(pnl_loop_t*);


static inline
void* pnl_loop_get_data(pnl_loop_t* l){
	return l->data;
}

static inline
void pnl_loop_set_data(pnl_loop_t* l, void* data){
	l->data = data;
}



#endif /*PNL_LOOP_H*/
