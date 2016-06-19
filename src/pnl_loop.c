/*
 * pnl_loop.c
 *
 *  Created on: 25.05.2016
 *      Author: philgras
 */

#include "pnl_loop.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define EVENT_BUFFER_SIZE 10

typedef struct epoll_event pnl_event_t;

static pnl_loop_t unix_loop;


static void update_timers(pnl_loop_t*);
static int deactivate(pnl_loop_t*, pnl_tcp_t*);
static void close_inactive(pnl_loop_t*);
static void handle_io(pnl_loop_t*, pnl_tcp_t*, int ioevents);
static void handle_server_io(pnl_loop_t*, pnl_tcp_t*, int ioevents);
static void handle_connection_io(pnl_loop_t*, pnl_tcp_t*, int ioevents);
static int reading_routine(pnl_loop_t*,pnl_tcpconn_t*);
static int writing_routine(pnl_loop_t*,pnl_tcpconn_t*) ;




pnl_loop_t* pnl_get_platform_loop(void){
	return &unix_loop;
}

void pnl_loop_init(pnl_loop_t* l){
	l->epollfd = INVALID_FD;
	pnl_list_init(&l->active_tcp);
	pnl_list_init(&l->inactive_tcp);
	l->system_time = pnl_get_system_time();
	l->poll_timeout = PNL_DEFAULT_TIMEOUT;
	l->running = 0;
}

int pnl_loop_create_server(pnl_loop_t* l,
							  pnl_tcpserver_t* server,
							  const char* ip,
							  const char* port,
							  on_close_cb on_server_close,
							  on_close_cb on_conn_close,
							  on_accept_cb on_accept){


	int rc;
	pnl_event_t event;

	pnl_tcpserver_init(server);
	rc = pnl_tcp_listen(server,ip,port);

	if(rc == PNL_OK){

		event.data.ptr = server;
		event.events = EPOLLIN | EPOLLET;
		rc = epoll_ctl(l->epollfd,EPOLL_CTL_ADD,server->tcpbase.socket_fd,&event);

		if(rc == 0){

			pnl_list_insert(&(l->active_tcp),&(server->tcpbase.node));
			server->connection_close_cb = on_conn_close;
			server->accept_cb = on_accept;
			server->tcpbase.close_cb = on_server_close;
			server->tcpbase.io_cb = handle_server_io;
			server->tcpbase.timer.last_action = l->system_time;
			server->tcpbase.timer.timeout = PNL_INFINITE_TIMEOUT;

		}else{
			int err = errno;

			server->tcpbase.inactive = 1;
			pnl_tcp_close((pnl_tcp_t*)server);

			PNL_TCP_ERROR(&server->tcpbase,PNL_EEVENTADD,err);
			rc = PNL_ERR;
		}
	}

	return rc;
}





int pnl_loop_remove_server(pnl_loop_t* l, pnl_tcpserver_t* server){
	return deactivate(l,(pnl_tcp_t*)server);
}





int pnl_loop_create_connection(pnl_loop_t* l,
							   pnl_tcpconn_t* conn,
							   const char* ip,
							   const char*  port,
							   on_close_cb on_close,
							   on_connect_cb on_connect){
	int rc, epoll_rc;
	pnl_event_t event;

	pnl_tcpconn_init(conn);
	rc = pnl_tcp_connect(conn,ip,port);

	if(rc == PNL_OK  || conn->tcpbase.error.pnl_ec == PNL_EWAIT){

		event.data.ptr = conn;
		event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
		epoll_rc = epoll_ctl(l->epollfd,EPOLL_CTL_ADD,conn->tcpbase.socket_fd,&event);

		if(epoll_rc == 0){

			pnl_list_insert(&(l->active_tcp),&(conn->tcpbase.node));
			conn->tcpbase.io_cb = handle_connection_io;
			conn->connect_cb = on_connect;
			conn->tcpbase.close_cb = on_close;
			conn->tcpbase.timer.last_action= l->system_time;
			conn->tcpbase.timer.timeout = PNL_DEFAULT_TIMEOUT;

			if(rc == PNL_OK){
				conn->connected = 1;
				if(conn->connect_cb(l,conn)!= PNL_OK){
					deactivate(l,(pnl_tcp_t*)conn);
				}
			}

			rc = PNL_OK;

		}else{
			int err = errno;

			conn->tcpbase.inactive = 1;
			pnl_tcp_close((pnl_tcp_t*)conn);

			PNL_TCP_ERROR(&conn->tcpbase,PNL_EEVENTADD,err);
			rc = PNL_ERR;
		}
	}


	return rc;
}

int pnl_loop_remove_connection(pnl_loop_t* l, pnl_tcpconn_t* conn){
	return deactivate(l,(pnl_tcp_t*)conn);
}


int pnl_loop_start_reading(pnl_loop_t* l, pnl_tcpconn_t* conn, on_ioevent_cb on_read, pnl_buffer_t* buf){
	int rc;

	if(!conn->tcpbase.inactive){

			conn->rbuffer = buf;
			conn->read_cb = on_read;
			conn->is_reading = 1;
			rc = reading_routine(l,conn);

	}else{
		PNL_TCP_ERROR(&conn->tcpbase, PNL_EINACTIVE,0);
		rc = PNL_ERR;
	}


	if(rc != PNL_OK){
		deactivate(l,(pnl_tcp_t*)conn);
	}

	return rc;
}

void pnl_loop_stop_reading( pnl_tcpconn_t* conn){

	if(conn->is_reading){
		conn->is_reading = 0;
		conn->read_cb = NULL;
		conn->rbuffer = NULL;
	}
}

int pnl_loop_start_writing(pnl_loop_t* l, pnl_tcpconn_t* conn, on_ioevent_cb on_write, pnl_buffer_t* buf){
	int rc;

		if(!conn->tcpbase.inactive){


				conn->write_cb = on_write;
				conn->wbuffer = buf;
				conn->is_writing = 1;
				rc = writing_routine(l,conn);


		}else{
			PNL_TCP_ERROR(&conn->tcpbase, PNL_EINACTIVE,0);
			rc = PNL_ERR;
		}


		if(rc != PNL_OK){
			deactivate(l,(pnl_tcp_t*)conn);
		}

		return rc;
}

void  pnl_loop_stop_writing(pnl_tcpconn_t* conn){
	if(conn->is_writing){
		conn->is_writing = 0;
		conn->write_cb = NULL;
		conn->wbuffer = NULL;
	}
}


int pnl_loop_start(pnl_loop_t* l, on_start onstart){

	int count;
	int rc;
	pnl_tcp_t* tcp_handle;

	l->epollfd = epoll_create1(0);

	if(l->epollfd == INVALID_FD){
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

		close(l->epollfd);
		l->epollfd = INVALID_FD;

	}

	return rc;
}


void pnl_loop_stop(pnl_loop_t* l){

	pnl_tcp_t* tcp_handle;

	l->running = 0;

	PNL_LIST_FOR_EACH(&l->active_tcp, iter){

		tcp_handle = PNL_LIST_ENTRY(iter,pnl_tcp_t,node);
		deactivate(l,tcp_handle);

	}

}


static void update_timers(pnl_loop_t* l){

	pnl_tcp_t* tcp_handle;
	pnl_timer_t t;
	l->poll_timeout = PNL_DEFAULT_TIMEOUT;

	PNL_LIST_FOR_EACH(&l->active_tcp, iter){

		tcp_handle = PNL_LIST_ENTRY(iter,pnl_tcp_t,node);
		t = tcp_handle->timer;

		if(t.timeout > PNL_INFINITE_TIMEOUT && l->system_time-t.last_action >= t.timeout){

			PNL_TCP_ERROR(tcp_handle,PNL_ETIMEOUT,0);
			deactivate(l,tcp_handle);

		}else if(l->poll_timeout > l->system_time-t.last_action ){
			l->poll_timeout = t.timeout - (l->system_time-t.last_action);
		}
	}

}

static int deactivate(pnl_loop_t* l, pnl_tcp_t* t){
	int rc = 0;
	pnl_event_t dummy;

	t->inactive = 1;
	pnl_list_remove(&t->node);
	pnl_list_insert(&l->inactive_tcp,&t->node);

	rc = epoll_ctl(l->epollfd,EPOLL_CTL_DEL,t->socket_fd,&dummy);

	if(rc == -1){
		PNL_TCP_CLEANUP_ERROR(t,PNL_EEVENTDEL,errno);
		rc = PNL_ERR;
	}
	return rc;
}

static void close_inactive(pnl_loop_t* l){

	pnl_list_t* head;
	pnl_tcp_t* tcp_handle;

	while(!pnl_list_is_empty(&l->inactive_tcp)){

		head = pnl_list_first(&l->inactive_tcp);
		tcp_handle = PNL_LIST_ENTRY(head,pnl_tcp_t,node);

		pnl_tcp_close(tcp_handle);

		if(tcp_handle->close_cb != NULL){
			tcp_handle->close_cb(l,tcp_handle);
		}

		pnl_list_remove(head);

		if(tcp_handle->allocated){
			pnl_free(tcp_handle);
		}
	}
}

static void handle_io(pnl_loop_t* l , pnl_tcp_t* t, int ioevents){

	if(ioevents & EPOLLHUP){
			PNL_TCP_ERROR(t,PNL_EHANGUP,0);
			deactivate(l,t);
	}else if( ioevents & EPOLLERR){
		PNL_TCP_ERROR(t,PNL_EEVENT,0);
		deactivate(l,t);
	}else {
		t->io_cb(l,t,ioevents);
	}
}

static void handle_server_io(pnl_loop_t* l, pnl_tcp_t* t, int ioevents){

	pnl_event_t event;
	int rc;
	pnl_tcpserver_t* server = (pnl_tcpserver_t*) t;
	pnl_tcpconn_t conn, *new_conn;


	if(ioevents & EPOLLIN){
		while(pnl_tcp_accept(server,&conn) == PNL_OK){

			new_conn = pnl_malloc(sizeof(pnl_tcpconn_t));
			if(new_conn == NULL){
				pnl_tcp_close((pnl_tcp_t*)&conn);
				PNL_TCP_ERROR(&server->tcpbase,PNL_EMALLOC,errno);
				break;
			}

			pnl_tcpconn_init(new_conn);
			new_conn->connected = 1;
			new_conn->tcpbase.allocated = 1;
			new_conn->tcpbase.socket_fd = conn.tcpbase.socket_fd;
			new_conn->tcpbase.io_cb = handle_connection_io;
			new_conn->tcpbase.close_cb = server->connection_close_cb;
			new_conn->tcpbase.timer.last_action = l->system_time;
			new_conn->tcpbase.timer.timeout = PNL_DEFAULT_TIMEOUT/2;
			pnl_list_insert(&l->active_tcp,&new_conn->tcpbase.node);

			event.data.ptr = new_conn;
			event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
			rc = epoll_ctl(l->epollfd,EPOLL_CTL_ADD,new_conn->tcpbase.socket_fd,&event);
			if(rc < 0){
				PNL_TCP_ERROR(&server->tcpbase,PNL_EEVENTADD,errno);
				pnl_tcp_close((pnl_tcp_t*)&conn);
				pnl_free(new_conn);
				break;
			}

			if(server->accept_cb(l,server,new_conn) != PNL_OK){
				deactivate(l,(pnl_tcp_t*)new_conn);
			}

		}

		if(server->tcpbase.error.pnl_ec != PNL_EWAIT){
			deactivate(l,(pnl_tcp_t*)server);
		}
	}
}

static void handle_connection_io(pnl_loop_t* l, pnl_tcp_t* t, int ioevents){

	pnl_tcpconn_t* conn = (pnl_tcpconn_t*)t;

    /*
     * handle close event initiated by the opposite peer
     */
	if(ioevents & EPOLLRDHUP){
		PNL_TCP_ERROR(t,PNL_EPEERCLO,0);
		deactivate(l,(pnl_tcp_t*)t);
		return;
	}

	/*
	 * handle all write events
	 */
	if(ioevents & EPOLLOUT){

		if(conn->connected){

			if(conn->is_writing && writing_routine(l,conn) != PNL_OK){
					deactivate(l,(pnl_tcp_t*)conn);
					return;
			}

		}else{

			if(pnl_tcp_connect_succeeded(conn) == PNL_OK
					&& conn->connect_cb(l,conn) == PNL_OK){
				conn->connected = 1;
			}else{
				deactivate(l,(pnl_tcp_t*)conn);
				return;
			}

		}
	}

	/*
	 * handle all read events
	 */
	if(ioevents & EPOLLIN){
		if(conn->connected && conn->is_reading){
			if(reading_routine(l,conn) != PNL_OK){
				deactivate(l,(pnl_tcp_t*)conn);
			}
		}
	}


}

static int reading_routine(pnl_loop_t* l, pnl_tcpconn_t* conn){

	int rc;

	do{
		rc = pnl_tcp_read(conn);

		if(rc == PNL_OK){
			rc = conn->read_cb(l,conn);
		}else if(rc == PNL_ERR && conn->tcpbase.error.pnl_ec == PNL_EWAIT){
			rc = PNL_OK;
			break;
		}else{
			rc = PNL_ERR;
		}

	}while(rc == PNL_OK);

	return rc;
}

static int writing_routine(pnl_loop_t* l,pnl_tcpconn_t* conn) {

	int rc;

		do{
			rc = pnl_tcp_write(conn);

			if(rc == PNL_OK){
				rc = conn->write_cb(l,conn);
			}else if(rc == PNL_ERR && conn->tcpbase.error.pnl_ec == PNL_EWAIT){
				rc = PNL_OK;
				break;
			}else{
				rc = PNL_ERR;
			}

		}while(rc == PNL_OK);

		return rc;
}

