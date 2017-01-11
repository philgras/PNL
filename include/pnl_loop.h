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

#define PNL_START_DAEMON_THREAD 1
#define PNL_START_SINGLE_THREADED 0

#define PNL_BASE_PTR(ptr) ((pnl_base_t*) (ptr))

typedef struct pnl_loop_s pnl_loop_t;

typedef struct pnl_base_s pnl_base_t;

typedef struct pnl_connection_s pnl_connection_t;

typedef struct pnl_server_s pnl_server_t;


typedef void (*on_close_cb)(pnl_loop_t *, pnl_base_t *);

typedef void (*on_error_cb)(pnl_loop_t *, pnl_base_t *, pnl_error_t *error);

typedef int (*on_write_cb)(pnl_loop_t *l, pnl_connection_t *);

typedef int (*on_read_cb)(pnl_loop_t *l, pnl_connection_t *, size_t read_bytes);

typedef int (*on_connect_cb)(pnl_loop_t *l, pnl_connection_t *);

typedef int (*on_accept_cb)(pnl_loop_t *l, pnl_server_t *, pnl_connection_t *);

typedef void (*on_start)(pnl_loop_t *l);


typedef struct {
    const char *host;
    const char *service;
    on_connect_cb on_connect;
    on_error_cb on_async_error;
    on_close_cb on_close;
    pnl_time_t timeout;
    void *data;
} pnl_connection_config_t;


typedef struct {
    const char *host;
    const char *service;
    on_accept_cb on_accept;
    on_error_cb on_async_server_error;
    on_error_cb on_async_connection_error;
    on_close_cb on_server_close;
    on_close_cb on_connection_close;
    pnl_time_t server_timeout;
    pnl_time_t connection_timeout;
    void *data;
} pnl_server_config_t;


/**
 *
 * @return
 */
pnl_loop_t *pnl_get_platform_loop(void);

/**
 *
 * @param loop
 */
void pnl_loop_init(pnl_loop_t *loop);

/**
 *
 * @param loop
 * @param onstart
 * @param deamon
 * @param error
 * @return
 */
int pnl_loop_start(pnl_loop_t *loop, on_start onstart, int deamon, pnl_error_t *error);

/**
 *
 * @param loop
 */
void pnl_loop_stop(pnl_loop_t *loop);


/**
 *
 * @param loop
 */
int pnl_loop_wait_for_daemon(pnl_loop_t *loop, pnl_error_t * error);

/**
 *
 * @param loop
 * @return
 */
void *pnl_loop_get_data(pnl_loop_t *loop);

/**
 *
 * @param loop
 * @param data
 */
void pnl_loop_set_data(pnl_loop_t *loop, void *data);

/**
 *
 * @param base
 * @param data
 */
void pnl_base_set_data(pnl_base_t *base, void *data);

/**
 *
 * @param base
 * @return
 */
void *pnl_base_get_data(pnl_base_t *base);

/**
 *
 * @param loop
 * @param server
 * @param config
 * @param error
 * @return
 */
int pnl_loop_add_server(pnl_loop_t *loop, pnl_server_t **server, const pnl_server_config_t *config, pnl_error_t *error);

/**
 *
 * @param loop
 * @param conn
 * @param config
 * @param error
 * @return
 */
int pnl_loop_add_connection(pnl_loop_t *loop, pnl_connection_t **conn, const pnl_connection_config_t *config,
                            pnl_error_t *error);

/**
 *
 * @param loop
 * @param conn
 * @param error
 * @return
 */
int pnl_loop_remove_connection(pnl_loop_t *loop, pnl_connection_t *conn, pnl_error_t *error);

/**
 *
 * @param loop
 * @param server
 * @param error
 * @return
 */
int pnl_loop_remove_server(pnl_loop_t *loop, pnl_server_t *server, pnl_error_t *error);

/**
 *
 * @param loop
 * @param conn
 * @param on_read
 * @param buffer
 * @param buffer_size
 * @param error
 * @return
 */
int pnl_loop_read(pnl_loop_t *loop, pnl_connection_t *conn, on_read_cb on_read, char *buffer, size_t buffer_size,
                  pnl_error_t *error);

/**
 *
 * @param loop
 * @param conn
 * @param on_write
 * @param buffer
 * @param buffer_size
 * @param error
 * @return
 */
int pnl_loop_write(pnl_loop_t *loop, pnl_connection_t *conn, on_write_cb on_write, char *buffer, size_t buffer_size,
                   pnl_error_t *error);


#endif /*PNL_LOOP_H*/

