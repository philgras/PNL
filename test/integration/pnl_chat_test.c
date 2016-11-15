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

typedef struct{
    pnl_connection_t conn;
    pnl_server_t server;
    pnl_connection_t* connptr;
    char buffer_a[BUFFERSIZE];
    char buffer_b[BUFFERSIZE];


}channel_t;


static const char* messages[] = {"abc def\thdkjd","a",TERMINATION};
static size_t message_index = 0;

static inline
void pnl_print_error(pnl_error_t* error) {
    printf("PNL error: %s\nSystem error: %s\n",
           pnl_strerror(error->pnl_ec),
           strerror(error->system_ec));
}

static inline
size_t read_line(char* buffer, size_t size){
    char c;
    char* pos = buffer;
    assert_true(strlen(messages[message_index])< size);
    strcpy(buffer,messages[message_index]);

    /*printf("Write something: ");
    while((c =  getchar()) != EOF && c != '\n'){
        if(pos-buffer < size-1){
            *pos++ = c;
        }
    }

    *pos++='\0';*/

    return strlen(buffer);

}


int connect_callback (pnl_loop_t *l, pnl_connection_t * connection);
int accept_cb(pnl_loop_t *loop, pnl_server_t *server, pnl_connection_t *conn);
int echo_read(pnl_loop_t *l, pnl_connection_t * connection, size_t read_bytes);
int echo_write(pnl_loop_t *l, pnl_connection_t * connection);
int user_read(pnl_loop_t *l, pnl_connection_t * connection, size_t read_bytes);
int user_write(pnl_loop_t *l, pnl_connection_t * connection);
void close_callback(pnl_loop_t * loop, pnl_base_t * base);

void start(pnl_loop_t *l){
    const char* hostname = "127.0.0.1";
    const char* service = "5005";
    int rc;

    channel_t* channel  = pnl_loop_get_data(l);

    rc = pnl_loop_add_server(l,&channel->server,NULL,service,close_callback,close_callback,accept_cb);

    if(rc == PNL_ERR){
        pnl_print_error(&PNL_BASE_PTR(&channel->server)->error);
        pnl_loop_stop(l);
        return;
    }

    rc = pnl_loop_add_connection(l,&channel->conn,hostname,service,close_callback,connect_callback);

    if(rc == PNL_ERR){
        pnl_print_error(&PNL_BASE_PTR(&channel->server)->error);
        pnl_loop_stop(l);
    }


}


int echo_write(pnl_loop_t *l, pnl_connection_t * connection){
    channel_t* channel = pnl_loop_get_data(l);
    printf("Written everything\n");
    return pnl_loop_read(l,connection,echo_read, channel->buffer_a,BUFFERSIZE-1);

}


int echo_read(pnl_loop_t *l, pnl_connection_t * connection, size_t read_bytes){

    channel_t* channel = pnl_loop_get_data(l);
    channel->buffer_a[read_bytes] = '\0';
    printf("Read: %s\n", channel->buffer_a);
    assert_string_equal(channel->buffer_a, messages[message_index]);
    if(strcmp(channel->buffer_a,TERMINATION) == 0){
        pnl_loop_remove_connection(l,connection);
        printf("Stopping server...\n");
        pnl_loop_stop(l);
        return PNL_OK;
    }
    return pnl_loop_write(l,connection,echo_write, channel->buffer_a,read_bytes);
}

int accept_cb(pnl_loop_t *loop, pnl_server_t *server, pnl_connection_t *conn){
    channel_t* channel = pnl_loop_get_data(loop);
    return pnl_loop_read(loop,conn,echo_read, channel->buffer_a,BUFFERSIZE-1);
}


int user_read(pnl_loop_t *l, pnl_connection_t * connection, size_t read_bytes){

    channel_t* channel = pnl_loop_get_data(l);
    channel->buffer_b[read_bytes] = '\0';
    printf("Read: %s\n", channel->buffer_b);
    assert_string_equal(channel->buffer_b, messages[message_index++]);
    size_t numberofbytes  = read_line(channel->buffer_b, BUFFERSIZE);
    return pnl_loop_write(l,connection,user_write, channel->buffer_b, numberofbytes);
}

int user_write(pnl_loop_t *l, pnl_connection_t * connection){
    channel_t* channel = pnl_loop_get_data(l);
    printf("Written everything\n");
    return pnl_loop_read(l,connection,user_read, channel->buffer_b,BUFFERSIZE-1);

}


int connect_callback (pnl_loop_t *l, pnl_connection_t * connection){
    channel_t* channel = pnl_loop_get_data(l);
    size_t numberofbytes  = read_line(channel->buffer_b, BUFFERSIZE);
    return pnl_loop_write(l,connection,user_write,channel->buffer_b, numberofbytes);
}


void close_callback(pnl_loop_t * loop, pnl_base_t * base){
    pnl_error_t* error = &base->error;
    if(error->pnl_ec != PNL_NOERR){

        pnl_print_error(&base->error);
        pnl_loop_stop(loop);

    }else{
        puts("Closed gracefully");
    }
}

void chat_test(void **state) {
    pnl_loop_t* loop;
    pnl_error_t error;
    channel_t channel;
    int rc;

    pnl_error_reset(&error);

    /*get loop*/
    loop = pnl_get_platform_loop();

    /*initialize loop and set data*/
    pnl_loop_init(loop);
    pnl_loop_set_data(loop,&channel);

    rc = pnl_loop_start(loop,start,&error);

    if(rc == PNL_ERR){
        pnl_print_error(&error);
    }
    assert_true(rc == PNL_OK);
}


int main(int argc, char *argv[]) {

    const struct CMUnitTest tests[] = { cmocka_unit_test(chat_test) };

    return cmocka_run_group_tests(tests,NULL,NULL);

}

