//
// Created by pgrassal on 09.11.16.
//

#include "pnl.h"
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#define BUFFERSIZE 512
#define TERMINATION "exit"
#define PNL_MAX_CONN 5000
#define PNL_WORD "word"

typedef struct {
    int message_index_a;
    int message_index_b;
    char buffer_a[BUFFERSIZE];
    char buffer_b[BUFFERSIZE];
} conn_data_t;


typedef struct {
    pnl_connection_t *conn[PNL_MAX_CONN];
    pnl_server_t *server;
    conn_data_t conn_data[PNL_MAX_CONN];
    int count1;
    int count2;
    int close_count;
} channel_t;


static const char *messages[] = {PNL_WORD, PNL_WORD, TERMINATION};


static inline
void pnl_print_error(pnl_error_t *error) {
    printf("PNL error: %s\nSystem error: %s\n",
           pnl_str_pnl_error(error),
           pnl_str_system_error(error));
}

static inline
size_t read_line(char *buffer, size_t size, int message_index) {
    assert_true(strlen(messages[message_index]) < size);
    strcpy(buffer, messages[message_index]);

    return strlen(buffer);

}


int connect_callback(pnl_loop_t *l, pnl_connection_t *connection);

int accept_cb(pnl_loop_t *loop, pnl_server_t *server, pnl_connection_t *conn);

int echo_read(pnl_loop_t *l, pnl_connection_t *connection, size_t read_bytes);

int echo_write(pnl_loop_t *l, pnl_connection_t *connection);

int user_read(pnl_loop_t *l, pnl_connection_t *connection, size_t read_bytes);

int user_write(pnl_loop_t *l, pnl_connection_t *connection);

void error_cb(pnl_loop_t *loop, pnl_base_t *base, pnl_error_t *error);

void close_cb(pnl_loop_t *loop, pnl_base_t *base);

void start(pnl_loop_t *l) {
    const char *hostname = "127.0.0.1";
    const char *service = "8080";
    int rc;
    conn_data_t *data;
    pnl_error_t error = PNL_ERROR_INIT;
    pnl_server_config_t sc = {
            .host = NULL,
            .service=service,
            .connection_timeout = PNL_DEFAULT_TIMEOUT,
            .server_timeout = PNL_INFINITE_TIMEOUT,
            .on_accept = accept_cb,
            .on_async_connection_error = error_cb,
            .on_async_server_error = error_cb,
            .on_connection_close = close_cb,
            .on_server_close = close_cb,
            .data = NULL
    };

    pnl_connection_config_t cc = {
            .host = hostname,
            .service=service,
            .timeout = PNL_DEFAULT_TIMEOUT,
            .on_connect = connect_callback,
            .on_async_error = error_cb,
            .on_close = close_cb,
            .data = NULL
    };

    channel_t *channel = pnl_loop_get_data(l);
    channel->count1 = 0;
    channel->count2 = 0;
    channel->close_count = 0;


    rc = pnl_loop_add_server(l, &channel->server, &sc, &error);

    if (rc == PNL_ERR) {
        pnl_print_error(&error);
        pnl_loop_stop(l);
        return;
    }

    for (int i = 0; i < PNL_MAX_CONN; ++i) {
        data = &channel->conn_data[i];
        data->message_index_a = 0;
        data->message_index_b = 0;
        cc.data = data;
        rc = pnl_loop_add_connection(l, &channel->conn[i], &cc, &error);
        if (rc == PNL_ERR) {
            pnl_print_error(&error);
            pnl_loop_stop(l);
            printf("%d", i);
            break;
        }
    }
}


int echo_write(pnl_loop_t *l, pnl_connection_t *connection) {
    pnl_error_t error = PNL_ERROR_INIT;
    conn_data_t *cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    if (strcmp(cd->buffer_a, TERMINATION) == 0) {
        pnl_loop_remove_connection(l, connection, &error);
        return PNL_OK;
    }

    if (pnl_loop_read(l, connection, echo_read, cd->buffer_a, BUFFERSIZE - 1, &error)) {
        pnl_print_error(&error);
        return PNL_ERR;
    }

    return PNL_OK;

}


int echo_read(pnl_loop_t *l, pnl_connection_t *connection, size_t read_bytes) {
    pnl_error_t error = PNL_ERROR_INIT;

    conn_data_t *cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    cd->buffer_a[read_bytes] = '\0';
    /*printf("Read: %s\n", cd->buffer_a);*/
    assert_string_equal(cd->buffer_a, messages[cd->message_index_a++]);

    if (pnl_loop_write(l, connection, echo_write, cd->buffer_a, read_bytes, &error)) {
        pnl_print_error(&error);
        return PNL_ERR;
    }

    return PNL_OK;
}

int accept_cb(pnl_loop_t *loop, pnl_server_t *server, pnl_connection_t *conn) {
    pnl_error_t error = PNL_ERROR_INIT;
    channel_t *channel = pnl_loop_get_data(loop);
    conn_data_t *cd = &channel->conn_data[channel->count2++];
    pnl_base_set_data(PNL_BASE_PTR(conn), cd);
    if (pnl_loop_read(loop, conn, echo_read, cd->buffer_a, BUFFERSIZE - 1, &error)) {
        pnl_print_error(&error);
        return PNL_ERR;
    }
    return PNL_OK;
}


int user_read(pnl_loop_t *l, pnl_connection_t *connection, size_t read_bytes) {
    pnl_error_t error = PNL_ERROR_INIT;
    channel_t *channel = pnl_loop_get_data(l);
    conn_data_t *cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    cd->buffer_b[read_bytes] = '\0';
    /*printf("Read: %s\n", cd->buffer_b);*/
    assert_string_equal(cd->buffer_b, messages[cd->message_index_b++]);
    if (strcmp(cd->buffer_b, TERMINATION) == 0) {
        pnl_loop_remove_connection(l, connection, &error);
        channel->count1 += 1;
        if (channel->count1 == PNL_MAX_CONN) {
            printf("Stopping server...\n");
            pnl_loop_stop(l);
        }
        return PNL_OK;
    }
    size_t numberofbytes = read_line(cd->buffer_b, BUFFERSIZE, cd->message_index_b);
    if (pnl_loop_write(l, connection, user_write, cd->buffer_b, numberofbytes, &error)) {
        pnl_print_error(&error);
        return PNL_ERR;
    }
    return PNL_OK;
}

int user_write(pnl_loop_t *l, pnl_connection_t *connection) {
    pnl_error_t error = PNL_ERROR_INIT;
    conn_data_t *cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    /*printf("Written everything\n");*/

    if (pnl_loop_read(l, connection, user_read, cd->buffer_b, BUFFERSIZE - 1, &error)) {
        pnl_print_error(&error);
        return PNL_ERR;
    }

    return PNL_OK;

}


int connect_callback(pnl_loop_t *l, pnl_connection_t *connection) {
    pnl_error_t error = PNL_ERROR_INIT;
    conn_data_t *cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    size_t numberofbytes = read_line(cd->buffer_b, BUFFERSIZE, cd->message_index_b);
    if (pnl_loop_write(l, connection, user_write, cd->buffer_b, numberofbytes, &error)) {
        pnl_print_error(&error);
        return PNL_ERR;

    }
    return PNL_OK;
}

void error_cb(pnl_loop_t *loop, pnl_base_t *base, pnl_error_t *error) {

    pnl_print_error(error);

}

void close_cb(pnl_loop_t *loop, pnl_base_t *base) {

    channel_t *channel = pnl_loop_get_data(loop);

    if (++channel->close_count == 2 * PNL_MAX_CONN) {

        pnl_loop_stop(loop);
    }

}

void echo_test(void **state) {
    int rc, count_A, count_B;
    pnl_loop_t *loop;
    pnl_time_t start_time, end_time;
    pnl_error_t error = PNL_ERROR_INIT;
    pnl_error_reset(&error);

    channel_t *channel = pnl_malloc(sizeof(channel_t));
    assert_true(channel != NULL);

    /*get loop*/
    loop = pnl_get_platform_loop();

    /*initialize loop and set data*/
    pnl_loop_init(loop);
    pnl_loop_set_data(loop, channel);
    start_time = pnl_get_system_time();
    rc = pnl_loop_start(loop, NULL, PNL_START_DAEMON_THREAD, &error);

    if (rc == PNL_ERR) {
        pnl_print_error(&error);
    } else {
        start(loop);
        rc = pnl_loop_wait_for_daemon(loop,&error);
    }

    end_time = pnl_get_system_time();
    printf("Time elapsed: %ld ms\n\n", end_time - start_time);


    count_A = count_B = 0;

    for (int i = 0; i < PNL_MAX_CONN; ++i) {
        if (channel->conn_data[i].message_index_a == sizeof(messages) / sizeof(char *)) ++count_A;
        if (channel->conn_data[i].message_index_b == sizeof(messages) / sizeof(char *)) ++count_B;
    }

    pnl_free(channel);
    printf("Count A: %d, Count B: %d\n\n", count_A, count_B);
    fflush(stdout);

    assert_true(count_A == PNL_MAX_CONN && count_B == PNL_MAX_CONN);
    assert_true(rc == PNL_OK);
}


int main(int argc, char *argv[]) {

    const struct CMUnitTest tests[] = {cmocka_unit_test(echo_test)};

    return cmocka_run_group_tests(tests, NULL, NULL);

}

