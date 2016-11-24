/*
 * pnl_loop.c
 *
 *  Created on: 25.05.2016
 *      Author: philgras
 */

#include "pnl_loop.h"
#include "pnl_tcp.h"
#include "pnl_system.h"

#include <errno.h>
#include <sys/epoll.h>
#include <pnl_error.h>


#define EVENT_BUFFER_SIZE 10

typedef struct epoll_event pnl_event_t;

typedef void (*on_iointernal_cb)(pnl_loop_t *l, pnl_base_t *, int ioevents);

enum pnl_io_tasks {
    NO_IO_TASK = 0,
    IO_TASK_CONNECT = 1 << 0,
    IO_TASK_READ = 1 << 1,
    IO_TASK_WRITE = 1 << 2,
    IO_TASK_ACCEPT = 1 << 3
};

/*internal structs - only the names are exposed to the API user*/


/*
 * BASE
 */
struct pnl_base_s {
    void *data;

    /*list interface*/
    pnl_list_t node;

    pnl_fd_t system_fd;
    /*timer*/
    pnl_timer_t timer;

    on_close_cb close_cb;

    on_iointernal_cb io_cb;

    pnl_list_t io_task_node;
    unsigned int io_tasks:4;

    pnl_error_t error;

    unsigned inactive:1;
};


/*
 * SERVER
 */
struct pnl_server_s {
    pnl_base_t base;
    on_accept_cb accept_cb;
    on_close_cb connection_close_cb;
};


/*
 * CONNECTION
 */
struct pnl_connection_s {

    pnl_base_t base;

    on_connect_cb connect_cb;

    on_read_cb read_cb;
    pnl_buffer_t rbuffer;

    on_write_cb write_cb;
    pnl_buffer_t wbuffer;

    unsigned int connected:1;
    unsigned int is_reading:1;
    unsigned int is_writing:1;
    unsigned int peer_closed:1;

};


/*
 * LOOP
 */
struct pnl_loop_s {
    /*PRIVATE*/
    pnl_fd_t epollfd;

    pnl_list_t io_task_tcp;
    pnl_list_t active_tcp;
    pnl_list_t inactive_tcp;

    pnl_time_t system_time;
    pnl_time_t poll_timeout;

    void *data;

    volatile unsigned int running:1;

};


/*global loop object*/
static pnl_loop_t linux_2_6_loop;

/*static function declarations*/
static void update_timers(pnl_loop_t *);

static int deactivate(pnl_loop_t *, pnl_base_t *);

static void close_inactive(pnl_loop_t *);

static void handle_io(pnl_loop_t *, pnl_base_t *, int ioevents);

static void handle_server_io(pnl_loop_t *, pnl_base_t *, int ioevents);

static void handle_connection_io(pnl_loop_t *, pnl_base_t *, int ioevents);

static int handle_read(pnl_loop_t *, pnl_connection_t *);

static int handle_write(pnl_loop_t *, pnl_connection_t *);

static int handle_connect(pnl_loop_t *, pnl_connection_t *);

static int handle_accept(pnl_loop_t *, pnl_server_t *);

static void process_io_tasks(pnl_loop_t *l);

/*static inline function definitions*/
static inline
void pnl_base_init(pnl_base_t *t) {
    t->system_fd = PNL_INVALID_FD;
    t->inactive = 0;
    t->close_cb = NULL;
    t->error.pnl_ec = PNL_NOERR;
    t->error.system_ec = 0;
    t->io_cb = NULL;
    t->timer.last_action = 0;
    t->timer.timeout = 0;
    t->data = NULL;
    pnl_list_init(&(t->node));
    pnl_list_init(&t->io_task_node);
    t->io_tasks = NO_IO_TASK;
}

static inline
void pnl_connection_init(pnl_connection_t *c) {
    pnl_base_init(&c->base);
    c->connected = 0;

    c->write_cb = NULL;
    c->read_cb = NULL;
    c->connect_cb = NULL;

    pnl_buffer_init(&c->rbuffer);
    pnl_buffer_init(&c->wbuffer);

    c->is_reading = 0;
    c->is_writing = 0;
    c->peer_closed = 0;
}

static inline
void pnl_server_init(pnl_server_t *s) {
    pnl_base_init(&s->base);
    s->connection_close_cb = NULL;
    s->accept_cb = NULL;
}


static inline
void pnl_loop_add_io_task(pnl_loop_t *l, pnl_base_t *base, unsigned char io_task) {
    base->io_tasks |= io_task;
    if (pnl_list_is_empty(&base->io_task_node))
        pnl_list_insert(&l->io_task_tcp, &base->io_task_node);
}

/*
 * API function definitions --> declarations in pnl_loop.h
 */


pnl_loop_t *pnl_get_platform_loop(void) {
    return &linux_2_6_loop;
}

void pnl_loop_init(pnl_loop_t *l) {
    l->epollfd = PNL_INVALID_FD;
    pnl_list_init(&l->active_tcp);
    pnl_list_init(&l->inactive_tcp);
    pnl_list_init(&l->io_task_tcp);
    l->system_time = pnl_get_system_time();
    l->poll_timeout = PNL_DEFAULT_TIMEOUT;
    l->running = 0;
    l->data = NULL;
}

int pnl_loop_add_server(pnl_loop_t *l, pnl_server_t **server, const char *hostname, const char *service,
                        on_close_cb on_server_close, on_close_cb on_conn_close, on_accept_cb on_accept,
                        pnl_error_t *error) {


    int rc;
    pnl_event_t event;
    pnl_server_t *new_server;

    if(!l->running){
        pnl_error_set(error,PNL_ENOTRUNING,0);
        return PNL_ERR;
    }

    new_server = pnl_malloc(sizeof(pnl_server_t));
    if (!new_server) {

        rc = PNL_ERR;
        pnl_error_set(error, PNL_EMALLOC, errno);

    } else {

        pnl_server_init(new_server);
        rc = pnl_tcp_listen(&new_server->base.system_fd, hostname, service, error);

        if (rc == PNL_OK) {

            event.data.ptr = new_server;
            event.events = EPOLLIN | EPOLLET;
            rc = epoll_ctl(l->epollfd, EPOLL_CTL_ADD, new_server->base.system_fd, &event);

            if (rc == 0) {

                pnl_list_insert(&l->active_tcp, &new_server->base.node);
                new_server->connection_close_cb = on_conn_close;
                new_server->accept_cb = on_accept;
                new_server->base.close_cb = on_server_close;
                new_server->base.io_cb = handle_server_io;
                new_server->base.timer.last_action = l->system_time;
                new_server->base.timer.timeout = PNL_INFINITE_TIMEOUT;

                *server = new_server;
                rc = PNL_OK;

            } else {
                int err = errno;
                pnl_tcp_close(&new_server->base.system_fd, error);
                pnl_error_set(error, PNL_EEVENTADD, err);
                pnl_free(new_server);
                rc = PNL_ERR;
            }
        }
    }

    return rc;
}

int pnl_loop_add_connection(pnl_loop_t *l, pnl_connection_t **conn, const char *hostname, const char *service,
                            on_close_cb on_close, on_connect_cb on_connect, pnl_error_t *error) {
    int rc, epoll_rc;
    pnl_event_t event;
    pnl_connection_t *new_conn;

    if(!l->running){
        pnl_error_set(error,PNL_ENOTRUNING,0);
        return PNL_ERR;
    }

    new_conn = pnl_malloc(sizeof(pnl_connection_t));
    if (!new_conn) {

        rc = PNL_ERR;
        pnl_error_set(error, PNL_EMALLOC, errno);

    } else {

        pnl_connection_init(new_conn);
        rc = pnl_tcp_connect(&new_conn->base.system_fd, hostname, service, error);

        if (rc == PNL_OK || error->pnl_ec == PNL_EWAIT) {

            pnl_error_reset(error);

            event.data.ptr = new_conn;
            event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
            epoll_rc = epoll_ctl(l->epollfd, EPOLL_CTL_ADD, new_conn->base.system_fd, &event);

            if (epoll_rc == 0) {

                pnl_list_insert(&l->active_tcp, &new_conn->base.node);
                new_conn->base.io_cb = handle_connection_io;
                new_conn->connect_cb = on_connect;
                new_conn->base.close_cb = on_close;
                new_conn->base.timer.last_action = l->system_time;
                new_conn->base.timer.timeout = PNL_DEFAULT_TIMEOUT;
                pnl_loop_add_io_task(l, PNL_BASE_PTR(new_conn), IO_TASK_CONNECT);

                *conn = new_conn;
                rc = PNL_OK;

            } else {
                int err = errno;
                pnl_tcp_close(&new_conn->base.system_fd, error);
                pnl_error_set(error, PNL_EEVENTADD, err);
                pnl_free(new_conn);
                rc = PNL_ERR;
            }
        }
    }


    return rc;
}

int pnl_loop_remove_connection(pnl_loop_t *l, pnl_connection_t *conn, pnl_error_t *error) {
    int rc = deactivate(l, (pnl_base_t *) conn);
    if (rc == PNL_ERR) *error = conn->base.error;
    return rc;
}


int pnl_loop_remove_server(pnl_loop_t *l, pnl_server_t *server, pnl_error_t *error) {
    int rc = deactivate(l, (pnl_base_t *) server);
    if (rc == PNL_ERR) *error = server->base.error;
    return rc;
}


int pnl_loop_read(pnl_loop_t *l, pnl_connection_t *conn, on_read_cb on_read, char *buffer,
                  size_t buffer_size, pnl_error_t *error) {

    int rc = PNL_OK;

    if (conn->is_reading) {
        pnl_error_set(error, PNL_EALREADY, 0);
        rc = PNL_ERR;
    } else if (!conn->base.inactive) {

        pnl_buffer_set_input(&conn->rbuffer, buffer, buffer_size);
        conn->is_reading = 1;
        conn->read_cb = on_read;
        pnl_loop_add_io_task(l, PNL_BASE_PTR(conn), IO_TASK_READ);

    } else {
        pnl_error_set(error, PNL_EINACTIVE, 0);
        rc = PNL_ERR;
    }


    return rc;
}


int pnl_loop_write(pnl_loop_t *l, pnl_connection_t *conn, on_write_cb on_write, char *buffer,
                   size_t buffer_size, pnl_error_t *error) {

    int rc = PNL_OK;

    if (conn->is_writing) {

        pnl_error_set(error, PNL_EALREADY, 0);
        rc = PNL_ERR;

    } else if (!conn->base.inactive) {

        pnl_buffer_set_output(&conn->wbuffer, buffer, buffer_size);
        conn->is_writing = 1;
        conn->write_cb = on_write;
        pnl_loop_add_io_task(l, PNL_BASE_PTR(conn), IO_TASK_WRITE);


    } else {
        pnl_error_set(error, PNL_EINACTIVE, 0);
        rc = PNL_ERR;
    }

    return rc;
}


int pnl_loop_start(pnl_loop_t *l, on_start onstart, pnl_error_t *error) {

    int count;
    int rc = PNL_OK;
    pnl_base_t *tcp_handle;

    l->epollfd = epoll_create1(0);

    if (l->epollfd == PNL_INVALID_FD) {

        pnl_error_set(error, PNL_EEVENT, errno);
        rc = PNL_ERR;

    } else {

        l->running = 1;
        onstart(l);
        while (l->running) {

            pnl_event_t events[EVENT_BUFFER_SIZE];
            /*execute open io tasks*/
            process_io_tasks(l);

            rc = epoll_wait(l->epollfd, events, EVENT_BUFFER_SIZE,(int) l->poll_timeout);
            if (rc < 0) {
                pnl_error_set(error, PNL_EEVENTWAIT, errno);
                pnl_loop_stop(l);
                rc = PNL_ERR;
            } else {

                /*set count*/
                count = rc;
                rc = PNL_OK;

                /*update timers*/
                l->system_time = pnl_get_system_time();
                update_timers(l);

                /*handle events*/
                for (int i = 0; i < count; ++i) {
                    tcp_handle = events[i].data.ptr;
                    if (!tcp_handle->inactive) {
                        handle_io(l, tcp_handle, events[i].events);
                    }
                }
            }
            /*close all inactive handles*/
            close_inactive(l);
        }

        close_inactive(l);
        pnl_close(l->epollfd);
        l->epollfd = PNL_INVALID_FD;
    }

    return rc;
}

void pnl_loop_stop(pnl_loop_t *l) {

    pnl_base_t *tcp_handle;

    l->running = 0;

    PNL_LIST_FOR_EACH(&l->active_tcp, iter) {

        tcp_handle = PNL_LIST_ENTRY(iter, pnl_base_t, node);
        deactivate(l, tcp_handle);

    }

}

void *pnl_loop_get_data(pnl_loop_t *l) {
    return l->data;
}

void pnl_loop_set_data(pnl_loop_t *l, void *data) {
    l->data = data;
}

void *pnl_base_get_data(pnl_base_t *b) {
    return b->data;
}

void pnl_base_set_data(pnl_base_t *b, void *data) {
    b->data = data;
}

/*static function definitions*/
static void update_timers(pnl_loop_t *l) {

    pnl_base_t *tcp_handle;
    pnl_timer_t t;
    l->poll_timeout = PNL_DEFAULT_TIMEOUT;

    PNL_LIST_FOR_EACH(&l->active_tcp, iter) {

        tcp_handle = PNL_LIST_ENTRY(iter, pnl_base_t, node);
        t = tcp_handle->timer;

        if (t.timeout > PNL_INFINITE_TIMEOUT && l->system_time - t.last_action >= t.timeout) {

            pnl_error_set(&tcp_handle->error, PNL_ETIMEOUT, 0);
            deactivate(l, tcp_handle);

        } else if (t.timeout > PNL_INFINITE_TIMEOUT &&
                l->poll_timeout < t.timeout - (l->system_time - t.last_action)) {
            l->poll_timeout = t.timeout - (l->system_time - t.last_action);
        }
    }

}

static int deactivate(pnl_loop_t *l, pnl_base_t *t) {
    int rc;
    pnl_event_t dummy;

    if (t->inactive) {
        pnl_error_set_cleanup(&t->error, PNL_EINACTIVE, 0);
        rc = PNL_ERR;
    } else {

        t->inactive = 1;
        pnl_list_remove(&t->node);
        pnl_list_remove(&t->io_task_node);
        t->io_tasks = NO_IO_TASK;
        pnl_list_insert(&l->inactive_tcp, &t->node);


        rc = epoll_ctl(l->epollfd, EPOLL_CTL_DEL, t->system_fd, &dummy);

        if (rc == -1) {
            pnl_error_set_cleanup(&t->error, PNL_EEVENTDEL, errno);
            rc = PNL_ERR;
        }
    }

    return rc;
}

static void close_inactive(pnl_loop_t *l) {

    pnl_list_t *head;
    pnl_base_t *tcp_handle;
    pnl_error_t *error;

    while (!pnl_list_is_empty(&l->inactive_tcp)) {

        head = pnl_list_first(&l->inactive_tcp);
        tcp_handle = PNL_LIST_ENTRY(head, pnl_base_t, node);

        pnl_tcp_close(&tcp_handle->system_fd, &tcp_handle->error);
        pnl_list_remove(head);

        error = NULL;
        if (tcp_handle->error.pnl_ec != PNL_NOERR) {
            error = &tcp_handle->error;
        }


        if (tcp_handle->close_cb != NULL) {
            tcp_handle->close_cb(l, tcp_handle, error);
        }

        pnl_free(tcp_handle);

    }
}

static void handle_io(pnl_loop_t *l, pnl_base_t *t, int ioevents) {

    if (ioevents & EPOLLHUP) {
        pnl_error_set(&t->error, PNL_EHANGUP, 0);
        deactivate(l, t);
    } else if (ioevents & EPOLLERR) {
        pnl_error_set(&t->error, PNL_EEVENT, 0);
        deactivate(l, t);
    } else {
        t->io_cb(l, t, ioevents);
    }
}

static void handle_server_io(pnl_loop_t *l, pnl_base_t *t, int ioevents) {
    if (ioevents & EPOLLIN) {
        pnl_loop_add_io_task(l, t, IO_TASK_ACCEPT);
    }
}

static void handle_connection_io(pnl_loop_t *l, pnl_base_t *t, int ioevents) {

    pnl_connection_t *conn = (pnl_connection_t *) t;

    if (ioevents & EPOLLOUT) {
        if (conn->connected) {
            if (conn->is_writing)
                pnl_loop_add_io_task(l, t, IO_TASK_WRITE);

        } else {
            pnl_loop_add_io_task(l, t, IO_TASK_CONNECT);
        }
    }

    if (ioevents & EPOLLIN) {
        if (conn->connected && conn->is_reading) {
            pnl_loop_add_io_task(l, t, IO_TASK_READ);
        }
    }

    if (ioevents & EPOLLRDHUP) {
        if (ioevents & EPOLLIN) {
            conn->peer_closed = 1;
        }else {
            pnl_error_set(&t->error, PNL_EPEERCLO, 0);
            deactivate(l, t);
        }
    }
}


static int handle_connect(pnl_loop_t *l, pnl_connection_t *conn) {

    int rc = pnl_tcp_connect_succeeded(&conn->base.system_fd, &conn->base.error);
    if (rc == PNL_OK) {

        rc = conn->connect_cb(l, conn);
        if (rc == PNL_OK || !conn->base.inactive) {
            conn->connected = 1;
        }
    } else if (conn->base.error.pnl_ec == PNL_EWAIT) {
        pnl_error_reset(&conn->base.error);
        rc = PNL_OK;
    }

    return rc;
}

static int handle_accept(pnl_loop_t *l, pnl_server_t *server) {
    pnl_event_t event;
    int rc;
    pnl_connection_t conn, *new_conn;

    while (pnl_tcp_accept(&server->base.system_fd,
                          &conn.base.system_fd,
                          &server->base.error) == PNL_OK) {

        new_conn = pnl_malloc(sizeof(pnl_connection_t));
        if (new_conn == NULL) {
            pnl_tcp_close(&conn.base.system_fd, &conn.base.error);
            pnl_error_set(&server->base.error, PNL_EMALLOC, errno);
            break;
        }

        pnl_connection_init(new_conn);
        new_conn->connected = 1;
        new_conn->base.system_fd = conn.base.system_fd;
        new_conn->base.io_cb = handle_connection_io;
        new_conn->base.close_cb = server->connection_close_cb;
        new_conn->base.timer.last_action = l->system_time;
        new_conn->base.timer.timeout = PNL_DEFAULT_TIMEOUT;
        pnl_list_insert(&l->active_tcp, &new_conn->base.node);

        event.data.ptr = new_conn;
        event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
        rc = epoll_ctl(l->epollfd, EPOLL_CTL_ADD, new_conn->base.system_fd, &event);

        if (rc < 0) {

            pnl_error_set(&new_conn->base.error, PNL_EEVENTADD, errno);
            deactivate(l, (pnl_base_t *) new_conn);
            continue;

        }

        if (server->accept_cb(l, server, new_conn) != PNL_OK) {
            deactivate(l, (pnl_base_t *) new_conn);
        }

    }

    if (server->base.error.pnl_ec == PNL_EWAIT) {
        rc = PNL_OK;
        pnl_error_reset(&server->base.error);
    } else {
        rc = PNL_ERR;
    }

    return rc;

}


static int handle_read(pnl_loop_t *l, pnl_connection_t *conn) {

    int rc;


    pnl_buffer_clear(&conn->rbuffer);
    rc = pnl_tcp_read(&conn->base.system_fd,
                      &conn->rbuffer,
                      &conn->base.error);


    if (rc == PNL_OK) {
        conn->is_reading = 0;
        rc = conn->read_cb(l, conn, conn->rbuffer.used);

    } else if (conn->base.error.pnl_ec == PNL_EWAIT) {
        pnl_error_reset(&conn->base.error);
        rc = PNL_OK;

    } else {
        rc = PNL_ERR;
    }

    return rc;
}

static int handle_write(pnl_loop_t *l, pnl_connection_t *conn) {

    int rc;


    rc = pnl_tcp_write(&conn->base.system_fd,
                       &conn->wbuffer,
                       &conn->base.error);

    if (rc == PNL_OK) {
        conn->is_writing = 0;
        rc = conn->write_cb(l, conn);

    } else if (conn->base.error.pnl_ec == PNL_EWAIT) {
        pnl_error_reset(&conn->base.error);
        rc = PNL_OK;

    } else {
        rc = PNL_ERR;
    }

    return rc;
}


static void process_io_tasks(pnl_loop_t *l) {

    pnl_base_t *base;
    pnl_list_t *head;
    while (!pnl_list_is_empty(&l->io_task_tcp)) {

        head = pnl_list_first(&l->io_task_tcp);
        base = PNL_LIST_ENTRY(head, pnl_base_t, io_task_node);

        /*
         * TODO: If more tasks are added, consider a list of io tasks per pnl_base_t instead of using enums
         *       struct io_task{
         *          io_task_fptr* handler;
         *          pnl_list_t node;
         *       }
         */

        while (base->io_tasks != NO_IO_TASK) {

            if (IO_TASK_READ & base->io_tasks) {

                pnl_connection_t* conn = (pnl_connection_t *) base;

                base->io_tasks ^= IO_TASK_READ;
                if (handle_read(l, conn) != PNL_OK) {
                    deactivate(l, base);
                    break;
                }

                if(conn->peer_closed){
                    if (IO_TASK_READ & base->io_tasks){
                        continue;
                    }else{
                        pnl_error_set(&base->error, PNL_EPEERCLO, 0);
                        deactivate(l, base);
                    }
                }
            }

            if (IO_TASK_CONNECT & base->io_tasks) {
                base->io_tasks ^= IO_TASK_CONNECT;
                if (handle_connect(l, (pnl_connection_t *) base) != PNL_OK) {
                    deactivate(l, base);
                    break;
                }
            }

            if (IO_TASK_WRITE & base->io_tasks) {
                base->io_tasks ^= IO_TASK_WRITE;
                if (handle_write(l, (pnl_connection_t *) base) != PNL_OK) {
                    deactivate(l, base);
                    break;
                }
            }

            if (IO_TASK_ACCEPT & base->io_tasks) {
                base->io_tasks ^= IO_TASK_ACCEPT;
                if (handle_accept(l, (pnl_server_t *) base) != PNL_OK) {
                    deactivate(l, base);
                    break;
                }
            }
        }

        pnl_list_remove(&base->io_task_node);
        base->timer.last_action = l->system_time;
    }

}
