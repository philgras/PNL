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
#include <assert.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <pthread.h>

#define EVENT_BUFFER_SIZE 1000
#define PNL_IO_TASK_MAX 2

#define PNL_IO_TASK_CONNECT handle_connect
#define PNL_IO_TASK_READ handle_read
#define PNL_IO_TASK_WRITE handle_write
#define PNL_IO_TASK_ACCEPT handle_accept

#define LOCK_LOOP(l) pthread_mutex_lock(&(l)->loop_mutex)
#define UNLOCK_LOOP(l) pthread_mutex_unlock(&(l)->loop_mutex)

#define LOCK_BASE(b) pthread_mutex_lock(&((pnl_base_t*)b)->mutex)
#define UNLOCK_BASE(b) pthread_mutex_unlock(&((pnl_base_t*)b)->mutex)


typedef struct epoll_event pnl_event_t;

typedef void (*on_iointernal_cb)(pnl_loop_t *l, pnl_base_t *, int ioevents);

typedef int (*io_task_handler)(pnl_loop_t *, pnl_base_t *);


/*internal structs - only the names are exposed to the API user*/

/*
 * BASE
 */
struct pnl_base_s {
    void *data;

    /*list interface*/
    pnl_list_t node;
    pnl_list_t io_task_node;
    pnl_list_t timer_node;

    pnl_fd_t system_fd;
    /*timer*/
    pnl_timer_t timer;

    on_error_cb async_error;
    on_close_cb close_cb;
    on_iointernal_cb io_cb;

    pthread_mutex_t mutex;
    pnl_error_t error;

    /*at most 2 simultaneous io_tasks*/
    io_task_handler io_tasks[PNL_IO_TASK_MAX];

    unsigned open_io_tasks:2;
    unsigned ready:1;
    unsigned inactive:1;
};


/*
 * SERVER
 */
struct pnl_server_s {
    pnl_base_t base;
    pnl_time_t connection_timeout;
    on_error_cb on_async_connection_error;
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
    pnl_base_t *wakeup_file;
    pnl_error_t error;

    pnl_list_t io_task_files;
    pnl_list_t active_files;
    pnl_list_t inactive_files;
    pnl_list_t timer_files;

    pnl_time_t system_time;
    pnl_time_t poll_timeout;

    void *data;

    pthread_t deamon_thread;
    pthread_mutex_t loop_mutex;
    pthread_mutex_t running_mutex;
    unsigned int deamon;
    unsigned int running;

};


/*global loop object*/
static pnl_loop_t linux_2_6_loop;

/*static function declarations*/
static void pnl_loop_on_startup_error(pnl_loop_t *l);

static void *main_routine(void *l);

static void update_timers(pnl_loop_t *);

static int deactivate(pnl_loop_t *, pnl_base_t *);

static void close_inactive(pnl_loop_t *);

static void handle_io(pnl_loop_t *, pnl_base_t *, int ioevents);

static void handle_server_io(pnl_loop_t *, pnl_base_t *, int ioevents);

static void handle_connection_io(pnl_loop_t *, pnl_base_t *, int ioevents);

static void handle_wakeup_io(pnl_loop_t *l, pnl_base_t *b, int ioevents);

static int handle_read(pnl_loop_t *, pnl_base_t *);

static int handle_write(pnl_loop_t *, pnl_base_t *);

static int handle_connect(pnl_loop_t *, pnl_base_t *);

static int handle_accept(pnl_loop_t *, pnl_base_t *);

static void process_io_tasks(pnl_loop_t *l);

static int add_wakeup_file(pnl_loop_t *loop, pnl_error_t *error);

static void wakeup_on_error(pnl_loop_t *l, pnl_base_t *b, pnl_error_t *error);


/*static inline function definitions*/
static inline
void pnl_base_init(pnl_base_t *t) {
    t->system_fd = PNL_INVALID_FD;
    t->inactive = 0;
    t->async_error = NULL;
    t->close_cb = NULL;
    t->error.pnl_ec = PNL_NOERR;
    t->error.system_ec = 0;
    t->io_cb = NULL;
    t->timer.last_action = 0;
    t->timer.timeout = 0;
    t->data = NULL;
    pnl_list_init(&t->timer_node);
    pnl_list_init(&t->node);
    pnl_list_init(&t->io_task_node);
    t->io_tasks[0] = t->io_tasks[1] = NULL;
    t->open_io_tasks = 0;
    t->ready = 0;
    t->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
}

static inline
void pnl_connection_init(pnl_connection_t *c) {
    pnl_base_init(&c->base);

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
    s->connection_timeout = 0;
    s->on_async_connection_error = NULL;
    s->connection_close_cb = NULL;
    s->accept_cb = NULL;
}

static inline
void pnl_base_push_io_task(pnl_base_t *base, io_task_handler handler) {

    /*check if IO task is already queued*/
    int ok = 1;
    for (int i = 0; i < base->open_io_tasks; ++i) {
        if (base->io_tasks[i] == handler) {
            ok = 0;
            break;
        }
    }

    if (ok) {
        base->io_tasks[base->open_io_tasks] = handler;
        ++base->open_io_tasks;
    }

}

static inline
void pnl_base_pop_io_task(pnl_base_t *base) {
    if (--base->open_io_tasks) {
        base->io_tasks[0] = base->io_tasks[1]; /*crap code*/
    }
}

static inline
void pnl_loop_add_io_task(pnl_loop_t *l, pnl_base_t *base, io_task_handler handler) {
    pnl_base_push_io_task(base, handler);
    if (pnl_list_is_empty(&base->io_task_node))
        pnl_list_insert(&l->io_task_files, &base->io_task_node);
}

static inline
void pnl_loop_wakeup(pnl_loop_t *loop) {
    if (loop->deamon && pthread_self() != loop->deamon_thread) {
        eventfd_write(loop->wakeup_file->system_fd, 1);
    }

}

static inline
int pnl_loop_atomic_is_running(pnl_loop_t *l) {
    int rc;
    pthread_mutex_lock(&l->running_mutex);
    rc = l->running;
    pthread_mutex_unlock(&l->running_mutex);
    return rc;
}

static inline
void pnl_loop_atomic_set_running(pnl_loop_t *l, unsigned int value) {
    pthread_mutex_lock(&l->running_mutex);
    l->running = value;
    pthread_mutex_unlock(&l->running_mutex);
}

/*
 * API function definitions --> declarations in pnl_loop.h
 */

pnl_loop_t *pnl_get_platform_loop(void) {
    return &linux_2_6_loop;
}

void pnl_loop_init(pnl_loop_t *l) {
    l->epollfd = PNL_INVALID_FD;
    l->deamon = 0;
    l->wakeup_file = NULL;
    l->error = (pnl_error_t) PNL_ERROR_INIT;
    pnl_list_init(&l->active_files);
    pnl_list_init(&l->inactive_files);
    pnl_list_init(&l->io_task_files);
    pnl_list_init(&l->timer_files);
    l->system_time = pnl_get_system_time();
    l->poll_timeout = PNL_INFINITE_TIMEOUT;
    l->running = 0;
    l->data = NULL;
    l->loop_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    l->running_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
}

int
pnl_loop_add_server(pnl_loop_t *loop, pnl_server_t **server, const pnl_server_config_t *config, pnl_error_t *error) {


    int rc;
    pnl_event_t event;
    pnl_server_t *new_server;

    new_server = pnl_malloc(sizeof(pnl_server_t));
    if (!new_server) {

        rc = PNL_ERR;
        pnl_error_set(error, PNL_EMALLOC, errno);

    } else {

        pnl_server_init(new_server);
        rc = pnl_tcp_listen(&new_server->base.system_fd, config->host, config->service, error);

        if (rc == PNL_OK) {

            LOCK_LOOP(loop);

            if (!pnl_loop_atomic_is_running(loop)) {

                pnl_error_set(error, PNL_ENOTRUNNING, 0);
                pnl_tcp_close(&new_server->base.system_fd, error);
                pnl_free(new_server);
                rc = PNL_ERR;

            } else {

                event.data.ptr = new_server;
                event.events = EPOLLIN | EPOLLET;
                rc = epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, new_server->base.system_fd, &event);

                if (rc == 0) {

                    pnl_list_insert(&loop->active_files, &new_server->base.node);
                    new_server->connection_timeout = config->connection_timeout;
                    new_server->connection_close_cb = config->on_connection_close;
                    new_server->accept_cb = config->on_accept;
                    new_server->on_async_connection_error = config->on_async_connection_error;
                    new_server->base.async_error = config->on_async_server_error;
                    new_server->base.close_cb = config->on_server_close;
                    new_server->base.io_cb = handle_server_io;
                    new_server->base.timer.last_action = loop->system_time;
                    new_server->base.timer.timeout = config->server_timeout;
                    if (new_server->base.timer.timeout > PNL_INFINITE_TIMEOUT) {
                        pnl_list_insert(&loop->timer_files, &new_server->base.timer_node);
                    }
                    new_server->base.data = config->data;
                    new_server->base.ready = 1;

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

            UNLOCK_LOOP(loop);

        } else {
            pnl_free(new_server);
        }
    }

    return rc;
}

int pnl_loop_add_connection(pnl_loop_t *loop, pnl_connection_t **conn, const pnl_connection_config_t *config,
                            pnl_error_t *error) {
    int rc, epoll_rc;
    pnl_event_t event;
    pnl_connection_t *new_conn;

    new_conn = pnl_malloc(sizeof(pnl_connection_t));
    if (!new_conn) {

        rc = PNL_ERR;
        pnl_error_set(error, PNL_EMALLOC, errno);

    } else {

        pnl_connection_init(new_conn);
        rc = pnl_tcp_connect(&new_conn->base.system_fd, config->host, config->service, error);

        if (rc == PNL_OK || error->pnl_ec == PNL_EWAIT) {

            pnl_error_reset(error);

            LOCK_LOOP(loop);

            if (!pnl_loop_atomic_is_running(loop)) {

                pnl_error_set(error, PNL_ENOTRUNNING, 0);
                pnl_tcp_close(&new_conn->base.system_fd, error);
                pnl_free(new_conn);
                rc = PNL_ERR;

            } else {

                event.data.ptr = new_conn;
                event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
                epoll_rc = epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, new_conn->base.system_fd, &event);

                if (epoll_rc == 0) {

                    pnl_list_insert(&loop->active_files, &new_conn->base.node);
                    new_conn->base.io_cb = handle_connection_io;
                    new_conn->connect_cb = config->on_connect;
                    new_conn->base.async_error = config->on_async_error;
                    new_conn->base.close_cb = config->on_close;
                    new_conn->base.timer.last_action = loop->system_time;
                    new_conn->base.timer.timeout = config->timeout;
                    if (new_conn->base.timer.timeout > PNL_INFINITE_TIMEOUT) {
                        pnl_list_insert(&loop->timer_files, &new_conn->base.timer_node);
                    }
                    new_conn->base.data = config->data;
                    pnl_loop_add_io_task(loop, PNL_BASE_PTR(new_conn), PNL_IO_TASK_CONNECT);
                    pnl_loop_wakeup(loop);

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

            UNLOCK_LOOP(loop);

        } else {
            pnl_free(new_conn);
        }
    }

    return rc;
}

int pnl_loop_remove_connection(pnl_loop_t *l, pnl_connection_t *conn, pnl_error_t *error) {

    LOCK_LOOP(l);
    int rc = deactivate(l, (pnl_base_t *) conn);
    if (rc == PNL_ERR) *error = conn->base.error;
    UNLOCK_LOOP(l);
    return rc;
}


int pnl_loop_remove_server(pnl_loop_t *l, pnl_server_t *server, pnl_error_t *error) {

    LOCK_LOOP(l);
    int rc = deactivate(l, (pnl_base_t *) server);
    if (rc == PNL_ERR) *error = server->base.error;
    UNLOCK_LOOP(l);
    return rc;
}


int pnl_loop_read(pnl_loop_t *l, pnl_connection_t *conn, on_read_cb on_read, char *buffer,
                  size_t buffer_size, pnl_error_t *error) {

    int rc = PNL_OK;


    LOCK_LOOP(l);

    if (!pnl_loop_atomic_is_running(l)) {
        pnl_error_set(error, PNL_ENOTRUNNING, 0);
        rc = PNL_ERR;
    } else if (!conn->base.ready) {
        pnl_error_set(error, PNL_ENOTCONNECTED, 0);
        rc = PNL_ERR;
    } else if (conn->is_reading) {
        pnl_error_set(error, PNL_EALREADY, 0);
        rc = PNL_ERR;
    } else if (conn->base.inactive) {
        pnl_error_set(error, PNL_EINACTIVE, 0);
        rc = PNL_ERR;
    } else {
        pnl_buffer_set_input(&conn->rbuffer, buffer, buffer_size);
        conn->is_reading = 1;
        conn->read_cb = on_read;
        pnl_loop_add_io_task(l, PNL_BASE_PTR(conn), PNL_IO_TASK_READ);
        pnl_loop_wakeup(l);
    }

    UNLOCK_LOOP(l);


    return rc;
}


int pnl_loop_write(pnl_loop_t *l, pnl_connection_t *conn, on_write_cb on_write, char *buffer,
                   size_t buffer_size, pnl_error_t *error) {

    int rc = PNL_OK;

    LOCK_LOOP(l);

    if (!pnl_loop_atomic_is_running(l)) {
        pnl_error_set(error, PNL_ENOTRUNNING, 0);
        rc = PNL_ERR;
    } else if (!conn->base.ready) {
        pnl_error_set(error, PNL_ENOTCONNECTED, 0);
        rc = PNL_ERR;
    } else if (conn->is_writing) {
        pnl_error_set(error, PNL_EALREADY, 0);
        rc = PNL_ERR;
    } else if (conn->base.inactive) {
        pnl_error_set(error, PNL_EINACTIVE, 0);
        rc = PNL_ERR;
    } else {
        pnl_buffer_set_output(&conn->wbuffer, buffer, buffer_size);
        conn->is_writing = 1;
        conn->write_cb = on_write;
        pnl_loop_add_io_task(l, PNL_BASE_PTR(conn), PNL_IO_TASK_WRITE);
        pnl_loop_wakeup(l);
    }

    UNLOCK_LOOP(l);

    return rc;
}


int pnl_loop_start(pnl_loop_t *l, on_start onstart, int deamon, pnl_error_t *error) {

    int rc;

    l->epollfd = epoll_create1(0);

    if (l->epollfd == PNL_INVALID_FD) {
        pnl_error_set(error, PNL_EEVENT, errno);
        rc = PNL_ERR;
        return rc;
    }

    rc = add_wakeup_file(l, error);
    if (rc == PNL_ERR) {
        pnl_close(l->epollfd);
        l->epollfd = PNL_INVALID_FD;
        return rc;
    }

    if (deamon) {
        l->deamon = 1;
        l->running = 1;
        rc = pthread_create(&l->deamon_thread, NULL, main_routine, l);

        if (rc != 0) {
            pnl_loop_on_startup_error(l);
            pnl_error_set(error, PNL_ETHREAD, rc);
            rc = PNL_ERR;

        } else {
            rc = PNL_OK;
        }

    } else {
        l->running = 1;
        if (onstart) onstart(l);

        main_routine(l);

        if (pnl_error_is_error(&l->error)) {
            rc = PNL_ERR;
            *error = l->error;
        }

    }

    return rc;
}

void pnl_loop_stop(pnl_loop_t *l) {

    pnl_base_t *tcp_handle;

    pnl_loop_atomic_set_running(l, 0);

    LOCK_LOOP(l);

    l->poll_timeout = 0;

    PNL_LIST_FOR_EACH(&l->active_files, iter) {

        tcp_handle = PNL_LIST_ENTRY(iter, pnl_base_t, node);
        deactivate(l, tcp_handle);

    }

    pnl_loop_wakeup(l);
    UNLOCK_LOOP(l);
}

int pnl_loop_wait_for_daemon(pnl_loop_t *loop, pnl_error_t *error) {
    int rc;
    if (loop->deamon) {
        void *ec = NULL;
        pthread_join(loop->deamon_thread, &ec);
        *error = *((pnl_error_t *) ec);
        rc = pnl_error_is_error(error) ? PNL_ERR : PNL_OK;
    } else {
        rc = PNL_ERR;
        pnl_error_set(error, PNL_ENODAEMON, 0);
    }

    return rc;
}

void *pnl_loop_get_data(pnl_loop_t *l) {
    void *data;
    LOCK_LOOP(l);
    data = l->data;
    UNLOCK_LOOP(l);
    return data;
}

void pnl_loop_set_data(pnl_loop_t *l, void *data) {
    LOCK_LOOP(l);
    l->data = data;
    UNLOCK_LOOP(l);
}

void *pnl_base_get_data(pnl_base_t *b) {
    void *data;
    LOCK_BASE(b);
    data = b->data;
    UNLOCK_BASE(b);
    return data;
}

void pnl_base_set_data(pnl_base_t *b, void *data) {
    LOCK_BASE(b);
    b->data = data;
    UNLOCK_BASE(b);
}

/*static function definitions*/
static void pnl_loop_on_startup_error(pnl_loop_t *l) {
    l->running = 0;

    pnl_tcp_close(&l->wakeup_file->system_fd, &l->wakeup_file->error);
    pnl_free(l->wakeup_file);
    l->wakeup_file = NULL;

    pnl_close(l->epollfd);
    l->epollfd = PNL_INVALID_FD;
}

static void *main_routine(void *loop) {

    int count;
    int rc;
    pnl_base_t *base;
    pnl_event_t events[EVENT_BUFFER_SIZE];
    pnl_loop_t *l = (pnl_loop_t *) loop;

    while (pnl_loop_atomic_is_running(l)) {

        /*execute open io tasks*/
        process_io_tasks(l);

        rc = epoll_wait(l->epollfd, events, EVENT_BUFFER_SIZE, (int) l->poll_timeout);
        if (rc < 0) {
            pnl_error_set(&l->error, PNL_EEVENTWAIT, errno);
            pnl_loop_stop(l);
        } else {

            /*set count*/
            count = rc;

            /*update timers*/
            update_timers(l);

            /*handle events*/
            LOCK_LOOP(l);
            for (int i = 0; i < count; ++i) {
                base = events[i].data.ptr;
                if (!base->inactive) {
                    handle_io(l, base, events[i].events);
                }
            }
            UNLOCK_LOOP(l);
        }
        /*close all inactive handles*/
        close_inactive(l);
    }

    close_inactive(l);
    pnl_close(l->epollfd);
    l->epollfd = PNL_INVALID_FD;

    return &l->error;
}


static void update_timers(pnl_loop_t *l) {

    pnl_base_t *tcp_handle;
    pnl_timer_t t;
    LOCK_LOOP(l);
    l->poll_timeout = PNL_INFINITE_TIMEOUT;
    l->system_time = pnl_get_system_time();

    PNL_LIST_FOR_EACH(&l->timer_files, iter) {

        tcp_handle = PNL_LIST_ENTRY(iter, pnl_base_t, timer_node);
        t = tcp_handle->timer;

        if (l->system_time - t.last_action >= t.timeout) {

            pnl_error_set(&tcp_handle->error, PNL_ETIMEOUT, 0);
            deactivate(l, tcp_handle);

        } else if (l->poll_timeout < t.timeout - (l->system_time - t.last_action)) {
            l->poll_timeout = t.timeout - (l->system_time - t.last_action);
        }
    }
    UNLOCK_LOOP(l);

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
        pnl_list_remove(&t->timer_node);
        while (t->open_io_tasks) {
            pnl_base_pop_io_task(t);
        }
        pnl_list_insert(&l->inactive_files, &t->node);


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

    LOCK_LOOP(l);

    while (!pnl_list_is_empty(&l->inactive_files)) {

        head = pnl_list_first(&l->inactive_files);
        tcp_handle = PNL_LIST_ENTRY(head, pnl_base_t, node);

        pnl_tcp_close(&tcp_handle->system_fd, &tcp_handle->error);
        pnl_list_remove(head);

        if (tcp_handle->error.pnl_ec != PNL_NOERR && tcp_handle->async_error != NULL) {
            UNLOCK_LOOP(l);
            tcp_handle->async_error(l, tcp_handle, &tcp_handle->error);
            LOCK_LOOP(l);
        }


        if (tcp_handle->ready && tcp_handle->close_cb != NULL) {
            UNLOCK_LOOP(l);
            tcp_handle->close_cb(l, tcp_handle);
            LOCK_LOOP(l);
        }

        pnl_free(tcp_handle);

    }

    UNLOCK_LOOP(l);
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
        pnl_loop_add_io_task(l, t, PNL_IO_TASK_ACCEPT);
    }
}

static void handle_connection_io(pnl_loop_t *l, pnl_base_t *t, int ioevents) {

    pnl_connection_t *conn = (pnl_connection_t *) t;

    if (ioevents & EPOLLOUT) {
        if (conn->base.ready) {
            if (conn->is_writing)
                pnl_loop_add_io_task(l, t, PNL_IO_TASK_WRITE);

        } else {
            pnl_loop_add_io_task(l, t, PNL_IO_TASK_CONNECT);
        }
    }

    if (ioevents & EPOLLIN) {
        if (conn->base.ready && conn->is_reading) {
            pnl_loop_add_io_task(l, t, PNL_IO_TASK_READ);
        }
    }

    if (ioevents & EPOLLRDHUP) {
        if (ioevents & EPOLLIN) {
            conn->peer_closed = 1;
        } else {
            pnl_error_set(&t->error, PNL_EPEERCLO, 0);
            deactivate(l, t);
        }
    }
}

static void handle_wakeup_io(pnl_loop_t *l, pnl_base_t *t, int ioevents) {
    eventfd_t fd;
    while (eventfd_read(t->system_fd, &fd) == 0);
    assert(errno == EAGAIN || errno == EWOULDBLOCK);
}

static int handle_connect(pnl_loop_t *l, pnl_base_t *base) {

    pnl_connection_t *conn = (pnl_connection_t *) base;

    int rc = pnl_tcp_connect_succeeded(&conn->base.system_fd, &conn->base.error);
    if (rc == PNL_OK) {
        conn->base.ready = 1;
        UNLOCK_LOOP(l);
        rc = conn->connect_cb(l, conn);
        LOCK_LOOP(l);
        if (rc != PNL_OK || conn->base.inactive) {
            conn->base.ready = 0;
            rc = PNL_ERR;
        }
    } else if (conn->base.error.pnl_ec == PNL_EWAIT) {
        pnl_error_reset(&conn->base.error);
        rc = PNL_OK;
    }

    return rc;
}

static int handle_accept(pnl_loop_t *l, pnl_base_t *base) {
    pnl_event_t event;
    int rc;
    pnl_connection_t conn, *new_conn;
    pnl_server_t *server = (pnl_server_t *) base;

    while (!server->base.inactive && pnl_tcp_accept(&server->base.system_fd,
                                                    &conn.base.system_fd,
                                                    &server->base.error) == PNL_OK) {

        new_conn = pnl_malloc(sizeof(pnl_connection_t));
        if (new_conn == NULL) {
            pnl_tcp_close(&conn.base.system_fd, &conn.base.error);
            pnl_error_set(&server->base.error, PNL_EMALLOC, errno);
            break;
        }

        pnl_connection_init(new_conn);
        new_conn->base.system_fd = conn.base.system_fd;
        new_conn->base.io_cb = handle_connection_io;
        new_conn->base.async_error = server->on_async_connection_error;
        new_conn->base.close_cb = server->connection_close_cb;
        new_conn->base.timer.last_action = l->system_time;
        new_conn->base.timer.timeout = server->connection_timeout;
        if (new_conn->base.timer.timeout > PNL_INFINITE_TIMEOUT) {
            pnl_list_insert(&l->timer_files, &new_conn->base.timer_node);
        }
        pnl_list_insert(&l->active_files, &new_conn->base.node);

        event.data.ptr = new_conn;
        event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
        rc = epoll_ctl(l->epollfd, EPOLL_CTL_ADD, new_conn->base.system_fd, &event);

        if (rc < 0) {

            pnl_error_set(&new_conn->base.error, PNL_EEVENTADD, errno);
            deactivate(l, (pnl_base_t *) new_conn); /*TODO may change this*/
            continue;

        }

        new_conn->base.ready = 1;
        UNLOCK_LOOP(l);
        rc = server->accept_cb(l, server, new_conn);
        LOCK_LOOP(l);

        if (rc != PNL_OK || new_conn->base.inactive) {
            new_conn->base.ready = 0;
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


static int handle_read(pnl_loop_t *l, pnl_base_t *base) {

    int rc;
    pnl_connection_t *conn = (pnl_connection_t *) base;

    pnl_buffer_clear(&conn->rbuffer);
    rc = pnl_tcp_read(&conn->base.system_fd,
                      &conn->rbuffer,
                      &conn->base.error);


    if (rc == PNL_OK) {
        conn->is_reading = 0;
        size_t used = conn->rbuffer.used;
        UNLOCK_LOOP(l);
        rc = conn->read_cb(l, conn, used);
        LOCK_LOOP(l);

    } else if (conn->base.error.pnl_ec == PNL_EWAIT) {
        pnl_error_reset(&conn->base.error);
        rc = PNL_OK;

    } else {
        rc = PNL_ERR;
    }

    if (conn->peer_closed && (conn->base.open_io_tasks == 0 || conn->base.io_tasks[0] != PNL_IO_TASK_READ)) {
        pnl_error_set(&conn->base.error, PNL_EPEERCLO, 0);
        rc = PNL_ERR;
    }

    return rc;
}

static int handle_write(pnl_loop_t *l, pnl_base_t *base) {

    int rc = PNL_OK;
    pnl_connection_t *conn = (pnl_connection_t *) base;

    if (!conn->peer_closed) {
        rc = pnl_tcp_write(&conn->base.system_fd,
                           &conn->wbuffer,
                           &conn->base.error);

        if (rc == PNL_OK) {
            conn->is_writing = 0;
            UNLOCK_LOOP(l);
            rc = conn->write_cb(l, conn);
            LOCK_LOOP(l);

        } else if (conn->base.error.pnl_ec == PNL_EWAIT) {
            pnl_error_reset(&conn->base.error);
            rc = PNL_OK;

        } else {
            rc = PNL_ERR;
        }
    }

    return rc;
}


static void process_io_tasks(pnl_loop_t *l) {

    pnl_base_t *base;
    pnl_list_t *head;
    io_task_handler handler;

    LOCK_LOOP(l);
    while (!pnl_list_is_empty(&l->io_task_files)) {

        head = pnl_list_first(&l->io_task_files);
        base = PNL_LIST_ENTRY(head, pnl_base_t, io_task_node);

        while (base->open_io_tasks) {
            handler = base->io_tasks[0];
            pnl_base_pop_io_task(base);
            if (handler(l, base) != PNL_OK) {
                deactivate(l, base);
                break;
            }
        }

        pnl_list_remove(&base->io_task_node);
        base->timer.last_action = l->system_time;
    }
    UNLOCK_LOOP(l);

}

static int add_wakeup_file(pnl_loop_t *loop, pnl_error_t *error) {

    int rc;
    pnl_event_t event;
    pnl_base_t *tf;

    tf = loop->wakeup_file = pnl_malloc(sizeof(pnl_base_t));
    if (tf == NULL) {
        pnl_error_set(error, PNL_EMALLOC, errno);
        return PNL_ERR;
    }

    pnl_base_init(tf);
    tf->system_fd = eventfd(0, EFD_NONBLOCK);
    if (tf->system_fd == PNL_INVALID_FD) {
        pnl_free(tf);
        loop->wakeup_file = NULL;
        pnl_error_set(error, PNL_EWAKEUPFD, errno);
        return PNL_ERR;
    }

    tf->ready = 1;
    tf->close_cb = NULL;
    tf->async_error = wakeup_on_error;
    tf->io_cb = handle_wakeup_io;
    tf->timer.last_action = loop->system_time;
    tf->timer.timeout = PNL_INFINITE_TIMEOUT;

    event.data.ptr = tf;
    event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    rc = epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, tf->system_fd, &event);

    if (rc != 0) {
        pnl_error_set(error, PNL_EEVENTADD, errno);
        pnl_tcp_close(&tf->system_fd, &tf->error);
        pnl_free(tf);
        loop->wakeup_file = NULL;
        rc = PNL_ERR;
    } else {
        pnl_list_insert(&loop->active_files, &tf->node);
        rc = PNL_OK;
    }

    return rc;

}

static void wakeup_on_error(pnl_loop_t *l, pnl_base_t *b, pnl_error_t *error) {
    pnl_loop_stop(l);
}