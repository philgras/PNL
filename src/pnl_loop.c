/*
 * pnl_loop.c
 *
 *  Created on: 25.05.2016
 *      Author: philgras
 */

#include "pnl_loop.h"
#include "pnl_tcp.h"
#include "pnl_sys_nio.h"

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>


#define EVENT_BUFFER_SIZE 10

typedef struct epoll_event pnl_event_t;


/*internal structs - only the names are exposed to the API user*/
/*
 * LOOP
 */
struct pnl_loop_s{
	/*PRIVATE*/
    pnl_fd_t epollfd;

    pnl_list_t active_tcp;
    pnl_list_t inactive_tcp;

    pnl_time_t system_time;
    pnl_time_t poll_timeout;

    void* data;

    volatile unsigned int running:1;

};



/*global loop object*/
static pnl_loop_t unix_loop;

/*static function declarations*/
static void update_timers(pnl_loop_t*);

static int deactivate(pnl_loop_t*, pnl_base_t*);

static void close_inactive(pnl_loop_t*);

static void handle_io(pnl_loop_t*, pnl_base_t*, int ioevents);

static void handle_server_io(pnl_loop_t*, pnl_base_t*, int ioevents);

static void handle_connection_io(pnl_loop_t*, pnl_base_t*, int ioevents);

static int reading_routine(pnl_loop_t*,pnl_connection_t*);

static int writing_routine(pnl_loop_t*,pnl_connection_t*) ;

static void close_connection_buffers(pnl_loop_t* l, pnl_base_t*);

/*static inline function definitions*/
static inline
void pnl_base_init(pnl_base_t* t){
	t->socket_fd  = PNL_INVALID_FD;
	t->inactive = 0;
	t->allocated = 0;
	t->close_cb = NULL;
	t->error.pnl_ec = PNL_NOERR;
	t->error.system_ec = 0;
	t->io_cb = NULL;
	t->timer.last_action = 0;
	t->timer.timeout = 0;
	t->internal_close_cb = NULL;
	t->data = NULL;
	pnl_list_init(&(t->node));
}

static inline
void pnl_connection_init(pnl_connection_t* c){
	pnl_base_init(&c->base);
	c->connected = 0;

	c->write_cb = NULL;
	c->read_cb = NULL;
	c->connect_cb = NULL;

	pnl_buffer_init(&c->rbuffer);
	pnl_buffer_init(&c->wbuffer);
	c->is_reading = 0;
	c->is_writing =0;
}

static inline
void pnl_server_init(pnl_server_t* s){
	pnl_base_init(&s->base);
	s->connection_close_cb = NULL;
	s->accept_cb = NULL;
}

/*API function definitions --> declarations in pnl_loop.h*/
pnl_loop_t* pnl_get_platform_loop(void){
	return &unix_loop;
}

void pnl_loop_init(pnl_loop_t* l){
	l->epollfd = PNL_INVALID_FD;
	pnl_list_init(&l->active_tcp);
	pnl_list_init(&l->inactive_tcp);
	l->system_time = pnl_get_system_time();
	l->poll_timeout = PNL_DEFAULT_TIMEOUT;
	l->running = 0;
	l->data = NULL;
}

int pnl_loop_add_server(pnl_loop_t* l,
							  pnl_server_t* server,
							  const char* ip,
							  const char* port,
							  on_close_cb on_server_close,
							  on_close_cb on_conn_close,
							  on_accept_cb on_accept){


	int rc;
	pnl_event_t event;

	pnl_server_init(server);
	rc = pnl_tcp_listen(&server->base.socket_fd, ip, port, &server->base.error);

	if(rc == PNL_OK){

		event.data.ptr = server;
		event.events = EPOLLIN | EPOLLET;
		rc = epoll_ctl(l->epollfd,EPOLL_CTL_ADD,server->base.socket_fd,&event);

		if(rc == 0){

			pnl_list_insert(&l->active_tcp,&server->base.node);
			server->connection_close_cb = on_conn_close;
			server->accept_cb = on_accept;
			server->base.close_cb = on_server_close;
			server->base.io_cb = handle_server_io;
			server->base.timer.last_action = l->system_time;
			server->base.timer.timeout = PNL_INFINITE_TIMEOUT;

		}else{
			int err = errno;

			server->base.inactive = 1;
			pnl_tcp_close(&server->base.socket_fd,&server->base.error);

			pnl_error_set(&server->base.error,PNL_EEVENTADD,err);
			rc = PNL_ERR;
		}
	}

	return rc;
}





int pnl_loop_remove_server(pnl_loop_t* l, pnl_server_t* server){
	return deactivate(l,(pnl_base_t*)server);
}





int pnl_loop_add_connection(pnl_loop_t* l,
							   pnl_connection_t* conn,
							   const char* ip,
							   const char*  port,
							   on_close_cb on_close,
							   on_connect_cb on_connect){
	int rc, epoll_rc;
	pnl_event_t event;

	pnl_connection_init(conn);
	rc = pnl_tcp_connect(&conn->base.socket_fd,ip,port, &conn->base.error);

	if(rc == PNL_OK  || conn->base.error.pnl_ec == PNL_EWAIT){

		event.data.ptr = conn;
		event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
		epoll_rc = epoll_ctl(l->epollfd,EPOLL_CTL_ADD,conn->base.socket_fd,&event);

		if(epoll_rc == 0){

			pnl_list_insert(&l->active_tcp,&conn->base.node);
			conn->base.io_cb = handle_connection_io;
			conn->connect_cb = on_connect;
			conn->base.close_cb = on_close;
			conn->base.internal_close_cb = close_connection_buffers;
			conn->base.timer.last_action= l->system_time;
			conn->base.timer.timeout = PNL_DEFAULT_TIMEOUT;

			if(rc == PNL_OK){
				conn->connected = 1;
				if(conn->connect_cb(l,conn)!= PNL_OK){
					deactivate(l,(pnl_base_t*)conn);
				}
			}

			rc = PNL_OK;

		}else{
			int err = errno;

			conn->base.inactive = 1;
			pnl_tcp_close(&conn->base.socket_fd,&conn->base.error);

			pnl_error_set(&conn->base.error,PNL_EEVENTADD,err);
			rc = PNL_ERR;
		}
	}


	return rc;
}

int pnl_loop_remove_connection(pnl_loop_t* l, pnl_connection_t* conn){
	return deactivate(l,(pnl_base_t*)conn);
}


int pnl_loop_start_reading(pnl_loop_t* l, pnl_connection_t* conn, on_ioevent_cb on_read, size_t buffer_size){
	int rc;

	if(conn->is_reading){
		pnl_error_set(&conn->base.error, PNL_EALREADY,0);
		rc = PNL_ERR;
	}else if(!conn->base.inactive){

			rc = pnl_buffer_alloc(&conn->rbuffer, buffer_size);
			if(rc == PNL_OK){
				conn->is_reading = 1;
				conn->read_cb = on_read;
				rc = reading_routine(l,conn);

				if(rc == PNL_ERR){
					pnl_loop_stop_reading(conn);
				}

			}else{
				pnl_error_set(&conn->base.error, PNL_EMALLOC,0);
			}

	}else{
		pnl_error_set(&conn->base.error, PNL_EINACTIVE,0);
		rc = PNL_ERR;
	}


	if(rc != PNL_OK){
		deactivate(l,(pnl_base_t*)conn);
	}

	return rc;
}

void pnl_loop_stop_reading( pnl_connection_t* conn){

	pnl_buffer_free(&conn->rbuffer);
	conn->is_reading = 0;
}

int pnl_loop_start_writing(pnl_loop_t* l, pnl_connection_t* conn, on_ioevent_cb on_write, size_t buffer_size){
	int rc;

	if(conn->is_writing){

		pnl_error_set(&conn->base.error, PNL_EALREADY,0);
			rc = PNL_ERR;

	}else if(!conn->base.inactive){

		rc = pnl_buffer_alloc(&conn->wbuffer, buffer_size);


		if(rc == PNL_OK){
			conn->write_cb = on_write;
			conn->is_writing =1;
			rc = writing_routine(l,conn);

			if(rc == PNL_ERR){
				pnl_loop_stop_writing(conn);
			}

		}else{
			pnl_error_set(&conn->base.error, PNL_EMALLOC,0);
		}


	}else{
		pnl_error_set(&conn->base.error, PNL_EINACTIVE,0);
		rc = PNL_ERR;
	}

	return rc;
}

void  pnl_loop_stop_writing(pnl_connection_t* conn){
	pnl_buffer_free(&conn->wbuffer);
	conn->is_writing= 0;
}


int pnl_loop_start(pnl_loop_t* l, on_start onstart){

	int count;
	int rc;
	pnl_base_t* tcp_handle;

	l->epollfd = epoll_create1(0);

	if(l->epollfd == PNL_INVALID_FD){
		rc = -PNL_EEVENT;
	}else{
		l->running = 1;

		onstart(l);

		while(l->running){

			pnl_event_t events[EVENT_BUFFER_SIZE];

			rc = epoll_wait(l->epollfd,events,EVENT_BUFFER_SIZE,(int) l->poll_timeout);

			if(rc<0){
				pnl_loop_stop(l);
				rc = PNL_ERR;
			}else{

				/*set count*/
				count  = rc;
				rc = PNL_OK;

				/*update timers*/
				l->system_time = pnl_get_system_time();
				update_timers(l);

				/*handle events*/
				for(int i = 0; i< count; ++i){
					tcp_handle =  events[i].data.ptr;
					if(!tcp_handle->inactive){
						handle_io(l,tcp_handle,events[i].events);
					}
				}
			}

			/*close all inactive handles*/
			close_inactive(l);
		}

		pnl_close(l->epollfd);
		l->epollfd = PNL_INVALID_FD;

	}

	return rc;
}


void pnl_loop_stop(pnl_loop_t* l){

	pnl_base_t* tcp_handle;

	l->running = 0;

	PNL_LIST_FOR_EACH(&l->active_tcp, iter){

		tcp_handle = PNL_LIST_ENTRY(iter,pnl_base_t,node);
		deactivate(l,tcp_handle);

	}

}

void* pnl_loop_get_data(pnl_loop_t* l){
	return l->data;
}


void pnl_loop_set_data(pnl_loop_t* l, void* data){
	l->data = data;
}

const pnl_error_t* pnl_base_get_error(const pnl_base_t* base){
	return &base->error;
}

void pnl_base_set_data(pnl_base_t* base, void* data){
	base->data = data;
}

void* pnl_base_get_data(const pnl_base_t* base){
	return base->data;
}


/*static function definitions*/
static void update_timers(pnl_loop_t* l){

	pnl_base_t* tcp_handle;
	pnl_timer_t t;
	l->poll_timeout = PNL_DEFAULT_TIMEOUT;

	PNL_LIST_FOR_EACH(&l->active_tcp, iter){

		tcp_handle = PNL_LIST_ENTRY(iter,pnl_base_t,node);
		t = tcp_handle->timer;

		if(t.timeout > PNL_INFINITE_TIMEOUT && l->system_time-t.last_action >= t.timeout){

			pnl_error_set(&tcp_handle->error,PNL_ETIMEOUT,0);
			deactivate(l,tcp_handle);

		}else if(l->poll_timeout > l->system_time-t.last_action ){
			l->poll_timeout = t.timeout - (l->system_time-t.last_action);
		}
	}

}

static int deactivate(pnl_loop_t* l, pnl_base_t* t){
	int rc = PNL_OK;
	pnl_event_t dummy;

	if(t->inactive){
		pnl_error_set_cleanup(&t->error,PNL_EALREADY,0);
		rc = PNL_ERR;
	}else{

		t->inactive = 1;
		pnl_list_remove(&t->node);
		pnl_list_insert(&l->inactive_tcp,&t->node);

		rc = epoll_ctl(l->epollfd,EPOLL_CTL_DEL,t->socket_fd,&dummy);

		if(rc == -1){
			pnl_error_set_cleanup(&t->error,PNL_EEVENTDEL,errno);
			rc = PNL_ERR;
		}
	}

	return rc;
}

static void close_inactive(pnl_loop_t* l){

	pnl_list_t* head;
	pnl_base_t* tcp_handle;

	while(!pnl_list_is_empty(&l->inactive_tcp)){

		head = pnl_list_first(&l->inactive_tcp);
		tcp_handle = PNL_LIST_ENTRY(head,pnl_base_t,node);

		pnl_tcp_close(&tcp_handle->socket_fd,&tcp_handle->error);
		pnl_list_remove(head);

		/*close buffers*/
		if(tcp_handle->internal_close_cb != NULL){
			tcp_handle->internal_close_cb(l,tcp_handle);
		}

		if(tcp_handle->close_cb != NULL){
			tcp_handle->close_cb(l,tcp_handle);
		}

		if(tcp_handle->allocated){
			pnl_free(tcp_handle);
		}
	}
}

static void handle_io(pnl_loop_t* l , pnl_base_t* t, int ioevents){

	if(ioevents & EPOLLHUP){
		pnl_error_set(&t->error,PNL_EHANGUP,0);
			deactivate(l,t);
	}else if( ioevents & EPOLLERR){
		pnl_error_set(&t->error,PNL_EEVENT,0);
		deactivate(l,t);
	}else {
		t->io_cb(l,t,ioevents);
	}
}

static void handle_server_io(pnl_loop_t* l, pnl_base_t* t, int ioevents){

	pnl_event_t event;
	int rc;
	pnl_server_t* server = (pnl_server_t*) t;
	pnl_connection_t conn, *new_conn;


	if(ioevents & EPOLLIN){
		while(pnl_tcp_accept(&server->base.socket_fd,
										 &conn.base.socket_fd,
										 &server->base.error) == PNL_OK){

			new_conn = pnl_malloc(sizeof(pnl_connection_t));
			if(new_conn == NULL){
				pnl_tcp_close(&conn.base.socket_fd,&conn.base.error);
				pnl_error_set(&t->error,PNL_EMALLOC,errno);
				break;
			}

			pnl_connection_init(new_conn);
			new_conn->connected = 1;
			new_conn->base.allocated = 1;
			new_conn->base.socket_fd = conn.base.socket_fd;
			new_conn->base.io_cb = handle_connection_io;
			new_conn->base.close_cb = server->connection_close_cb;
			new_conn->base.internal_close_cb = close_connection_buffers;
			new_conn->base.timer.last_action = l->system_time;
			new_conn->base.timer.timeout = PNL_DEFAULT_TIMEOUT;
			pnl_list_insert(&l->active_tcp,&new_conn->base.node);

			event.data.ptr = new_conn;
			event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
			rc = epoll_ctl(l->epollfd,EPOLL_CTL_ADD,new_conn->base.socket_fd,&event);

			if(rc < 0){

				pnl_error_set(&server->base.error,PNL_EEVENTADD,errno);
				pnl_tcp_close(&conn.base.socket_fd,&conn.base.error);
				pnl_free(new_conn);
				break;

			}

			if(server->accept_cb(l,server,new_conn) != PNL_OK){
				deactivate(l,(pnl_base_t*)new_conn);
			}

		}

		if(server->base.error.pnl_ec != PNL_EWAIT){
			deactivate(l,(pnl_base_t*)server);
		}else{
			pnl_error_reset(&server->base.error);
		}
	}
}

static void handle_connection_io(pnl_loop_t* l, pnl_base_t* t, int ioevents){

	int rc;
	pnl_connection_t* conn = (pnl_connection_t*)t;


	/*
	 * handle all write events
	 */
	if(ioevents & EPOLLOUT){

		if(conn->connected){

			if(conn->is_writing ){

				rc = writing_routine(l,conn);

				if(rc!= PNL_OK || conn->base.inactive){
					goto deactivate;
				}

			}

		}else{

			if(pnl_tcp_connect_succeeded(&conn->base.socket_fd, &conn->base.error) == PNL_OK){

				rc = conn->connect_cb(l,conn);

				if( rc == PNL_OK || !conn->base.inactive){

					conn->connected = 1;

				}else{

					goto deactivate;

				}
			}
		}
	}

	/*
	 * handle all read events
	 */
	if(ioevents & EPOLLIN){

		if(conn->connected && conn->is_reading){

			rc = reading_routine(l,conn);

			if(rc != PNL_OK || conn->base.inactive){
				goto deactivate;
			}
		}
	}


   /*
	 * handle close event initiated by the opposite peer
	 * IMPORTANT this event must be handled after possible read events
	 * because data might accidentally be ignored in the OS buffer used for reading
	 */
	if(ioevents & EPOLLRDHUP){
		pnl_error_set(&t->error,PNL_EPEERCLO,0);
		goto deactivate;
	}

	pnl_error_reset(&conn->base.error);
	goto end;

	deactivate:
		deactivate(l,(pnl_base_t*)conn);

	end:	return;
}

static int reading_routine(pnl_loop_t* l, pnl_connection_t* conn){

	int rc;

	do{
		pnl_buffer_clear(&conn->rbuffer);
		rc = pnl_tcp_read(&conn->base.socket_fd,
				 	 	 	 	 	&conn->rbuffer,
									&conn->base.error);

		if(rc == PNL_OK){
			rc = conn->read_cb(l,conn,&conn->rbuffer);

		}else if(rc == PNL_ERR && conn->base.error.pnl_ec == PNL_EWAIT){
			rc = PNL_OK;
			break;
		}else{
			rc = PNL_ERR;
		}

	}while(rc == PNL_OK && conn->is_reading);

	return rc;
}

static int writing_routine(pnl_loop_t* l,pnl_connection_t* conn) {

	int rc;

		do{
			rc = pnl_tcp_write(&conn->base.socket_fd,
										 &conn->wbuffer,
										 &conn->base.error);

			if(rc == PNL_OK){

				pnl_buffer_clear(&conn->wbuffer);
				rc = conn->write_cb(l,conn,&conn->wbuffer);
				pnl_buffer_reset_position(&conn->wbuffer);

			}else if(rc == PNL_ERR && conn->base.error.pnl_ec == PNL_EWAIT){
				rc = PNL_OK;
				break;
			}else{
				rc = PNL_ERR;
			}

		}while(rc == PNL_OK && conn->is_writing);

		return rc;
}

static void close_connection_buffers(pnl_loop_t* l, pnl_base_t* t){

	pnl_connection_t * conn = (pnl_connection_t *)t;
	if(conn->is_reading){
		pnl_loop_stop_reading(conn);
	}
	if(conn->is_writing){
		pnl_loop_stop_writing(conn);
	}

}
