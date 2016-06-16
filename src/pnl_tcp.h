/*
 * pnl_tcp.h
 *
 *  Created on: 27.05.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_TCP_H_
#define SRC_PNL_TCP_H_

#include "pnl.h"
#include "pnl_list.h"
#include <unistd.h>


#define PNL_TCP_ERROR(base,p_ec,sys_ec)			\
				(base)->error.pnl_ec = p_ec;					\
				(base)->error.system_ec = sys_ec

#define PNL_TCP_CLEANUP_ERROR(base,p_ec,sys_ec)			\
				if((base)->error.pnl_ec == 0){									\
					PNL_TCP_ERROR((base),p_ec,sys_ec);					\
				}

typedef struct pnl_tcpconn_s pnl_tcpconn_t;
typedef struct pnl_tcp_s pnl_tcp_t;
typedef struct pnl_loop_s pnl_loop_t;
typedef struct pnl_tcpserver_s pnl_tcpserver_t;

/*
 * CALLBACKS
 */
typedef void (*on_close_cb)(pnl_loop_t*,pnl_tcp_t*);
typedef void (*on_iointernal_cb)(pnl_loop_t* l, pnl_tcp_t*, int ioevents);
typedef int (*on_ioevent_cb)(pnl_loop_t* l, pnl_tcpconn_t*);
typedef int (*on_connect_cb)(pnl_loop_t* l, pnl_tcpconn_t*);
typedef int (*on_accept_cb)(pnl_loop_t* l, pnl_tcpserver_t*, pnl_tcpconn_t*);


/*
 * TCP
 */
struct pnl_tcp_s{
	/*list interface*/
	pnl_list_t node;

	pnl_fd_t socket_fd;
	/*timer*/
	pnl_timer_t timer;

	on_close_cb close_cb;

	on_iointernal_cb io_cb;

	pnl_error_t error;

	unsigned inactive:1;
	unsigned allocated:1;
};


/*
 * TCPSERVER
 */

struct pnl_tcpserver_s{

	 /*PRIVATE*/
	pnl_tcp_t tcpbase;

    on_accept_cb accept_cb;
    on_close_cb connection_close_cb;

    /*PUBLIC*/
    void* data;

};



/*
 * TCPCONNECTION
 */
struct pnl_tcpconn_s{

	/*PRIVATE - internal use only*/
	pnl_tcp_t tcpbase;

	on_connect_cb connect_cb;

	on_ioevent_cb read_cb;
	pnl_buffer_t* rbuffer;

	on_ioevent_cb write_cb;
	pnl_buffer_t* wbuffer;

	unsigned int connected:1;
	unsigned int is_reading:1;
	unsigned int is_writing:1;

	/*PUBLIC*/
	void* data;
};

int pnl_tcp_connect(pnl_tcpconn_t*, const char* ip, const char* port);
int pnl_tcp_connect_succeeded(pnl_tcpconn_t* );
int pnl_tcp_listen(pnl_tcpserver_t*, const char* ip, const char* port);
int pnl_tcp_accept(pnl_tcpserver_t*, pnl_tcpconn_t* conn);
int pnl_tcp_close(pnl_tcp_t*t);
int pnl_tcp_read(pnl_tcpconn_t*);
int pnl_tcp_write(pnl_tcpconn_t*);

static inline
void pnl_tcp_init(pnl_tcp_t* t){
	t->socket_fd  = INVALID_FD;
	t->inactive = 0;
	t->allocated = 0;
	t->close_cb = NULL;
	t->error.pnl_ec = PNL_NOERR;
	t->error.system_ec = 0;
	t->io_cb = NULL;
	t->timer.last_action = 0;
	t->timer.timeout = 0;
	pnl_list_init(&(t->node));
}

static inline
void pnl_tcpconn_init(pnl_tcpconn_t* c){
	pnl_tcp_init(&c->tcpbase);
	c->connected = 0;
	c->data = NULL;

	c->write_cb = NULL;
	c->read_cb = NULL;
	c->connect_cb = NULL;

	c->rbuffer = NULL;
	c->wbuffer = NULL;
	c->is_reading = 0;
	c->is_writing =0;
}

static inline
void pnl_tcpserver_init(pnl_tcpserver_t* s){
	pnl_tcp_init(&s->tcpbase);
	s->data = NULL;
	s->connection_close_cb = NULL;
	s->accept_cb = NULL;
}

#endif /* SRC_PNL_TCP_H_ */
