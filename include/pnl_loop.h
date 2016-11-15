/*
 * pnl_loop.h
 *
 *  Created on: 30.07.2016
 *      Author: philgras
 */


#ifndef PNL_LOOP_H
#define PNL_LOOP_H

#include "pnl_common.h"
#include "pnl_list.h"

#include <inttypes.h>

#define PNL_BASE_PTR(ptr) ((pnl_base_t*) (ptr))

typedef struct pnl_loop_s pnl_loop_t;
typedef struct pnl_base_s pnl_base_t;
typedef struct pnl_connection_s pnl_connection_t;
typedef struct pnl_server_s pnl_server_t;

typedef void (*on_close_cb)(pnl_loop_t *, pnl_base_t *);

typedef int (*on_write_cb)(pnl_loop_t *l, pnl_connection_t *);

typedef int (*on_read_cb)(pnl_loop_t *l, pnl_connection_t *, size_t read_bytes);

typedef int (*on_connect_cb)(pnl_loop_t *l, pnl_connection_t *);

typedef int (*on_accept_cb)(pnl_loop_t *l, pnl_server_t *, pnl_connection_t *);

typedef void (*on_start)(pnl_loop_t *l);

/*internal types*/
typedef void (*on_iointernal_cb)(pnl_loop_t *l, pnl_base_t *, int ioevents);

/*
 * BASE
 */
struct pnl_base_s {
    void *data;

    /*list interface*/
    pnl_list_t node;

    pnl_fd_t socket_fd;
    /*timer*/
    pnl_timer_t timer;

    on_close_cb close_cb;

    on_iointernal_cb io_cb;

    pnl_list_t io_task_node;
    unsigned int io_tasks:4;

    pnl_error_t error;

    unsigned inactive:1;
    unsigned allocated:1;
};


/*
 * SERVER
 */
struct pnl_server_s {

    /*PRIVATE*/
    pnl_base_t base;

    on_accept_cb accept_cb;
    on_close_cb connection_close_cb;

};


/*
 * CONNECTION
 */
struct pnl_connection_s {

    /*PRIVATE - internal use only*/
    pnl_base_t base;

    on_connect_cb connect_cb;

    on_read_cb read_cb;
    pnl_buffer_t rbuffer;

    on_write_cb write_cb;
    pnl_buffer_t wbuffer;

    unsigned int connected:1;
    unsigned int is_reading:1;
    unsigned int is_writing:1;

};

pnl_loop_t *pnl_get_platform_loop(void);

void pnl_loop_init(pnl_loop_t *);

int pnl_loop_add_server(pnl_loop_t *l, pnl_server_t *server, const char *host, const char *service,
                        on_close_cb on_server_close, on_close_cb on_conn_close, on_accept_cb on_accept);

int pnl_loop_remove_server(pnl_loop_t *l, pnl_server_t *);

int pnl_loop_add_connection(pnl_loop_t *l, pnl_connection_t *conn, const char *host, const char *service,
                            on_close_cb on_close, on_connect_cb on_connect);

int pnl_loop_remove_connection(pnl_loop_t *l, pnl_connection_t *conn);

int pnl_loop_read(pnl_loop_t *l, pnl_connection_t *conn, on_read_cb on_read, char *buffer, size_t buffer_size);

int pnl_loop_write(pnl_loop_t *l, pnl_connection_t *conn, on_write_cb on_write, char *buffer, size_t buffer_size);

int pnl_loop_start(pnl_loop_t *, on_start onstart, pnl_error_t* error);

void pnl_loop_stop(pnl_loop_t *);

void *pnl_loop_get_data(pnl_loop_t *l);

void pnl_loop_set_data(pnl_loop_t *l, void *data);



#endif /*PNL_LOOP_H*/

