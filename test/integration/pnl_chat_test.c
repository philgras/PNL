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
#define PNL_MAX_CONN 1000

typedef struct{
    int message_index_a;
    int message_index_b;
    char buffer_a[BUFFERSIZE];
    char buffer_b[BUFFERSIZE];




}conn_data_t;


typedef struct{
    pnl_connection_t* conn[PNL_MAX_CONN];
    pnl_server_t* server;
    conn_data_t conn_data[PNL_MAX_CONN];
    int count1;
    int count2;
    int close_count;
}channel_t;


#define PNL_WORD "WTF"


static const char* messages[] = {PNL_WORD,PNL_WORD,TERMINATION};


static inline
void pnl_print_error(pnl_error_t* error) {
    printf("PNL error: %s\nSystem error: %s\n",
           pnl_strerror(error->pnl_ec),
           strerror(error->system_ec));
}

static inline
size_t read_line(char* buffer, size_t size, int message_index){
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
void close_callback(pnl_loop_t * loop, pnl_base_t * base, pnl_error_t* error);

void start(pnl_loop_t *l){
    const char* hostname = "127.0.0.1";
    const char* service = "5005";
    int rc;
    conn_data_t* data;
    pnl_error_t error  = PNL_ERROR_INIT;

    channel_t* channel  = pnl_loop_get_data(l);
    channel->count1= 0;
    channel->count2 = 0;
    channel->close_count = 0;

    rc = pnl_loop_add_server(l,&channel->server,NULL,service,close_callback,close_callback,accept_cb, &error);

    if(rc == PNL_ERR){
        pnl_print_error(&error);
        pnl_loop_stop(l);
        return;
    }

    for(int i = 0; i < PNL_MAX_CONN; ++i) {
        rc = pnl_loop_add_connection(l, &channel->conn[i], hostname, service, close_callback, connect_callback, &error);
        data = &channel->conn_data[i];
        data->message_index_a = 0;
        data->message_index_b = 0;
        pnl_base_set_data(PNL_BASE_PTR(channel->conn[i]),data);
        if (rc == PNL_ERR) {
            pnl_print_error(&error);
            pnl_loop_stop(l);
        }
    }
}


int echo_write(pnl_loop_t *l, pnl_connection_t * connection){
    pnl_error_t* error;
    conn_data_t* cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    channel_t* channel = pnl_loop_get_data(l);
    if(strcmp(cd->buffer_a,TERMINATION) == 0){
        pnl_loop_remove_connection(l,connection,error);
        return PNL_OK;
    }
    return pnl_loop_read(l,connection,echo_read, cd->buffer_a,BUFFERSIZE-1, error);

}


int echo_read(pnl_loop_t *l, pnl_connection_t * connection, size_t read_bytes){
    pnl_error_t* error;

    conn_data_t* cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    cd->buffer_a[read_bytes] = '\0';
    /*printf("Read: %s\n", cd->buffer_a);*/
    assert_string_equal(cd->buffer_a, messages[cd->message_index_a++]);

    return pnl_loop_write(l,connection,echo_write, cd->buffer_a,read_bytes,error);
}

int accept_cb(pnl_loop_t *loop, pnl_server_t *server, pnl_connection_t *conn){
    pnl_error_t* error;
    channel_t* channel = pnl_loop_get_data(loop);
    conn_data_t* cd = &channel->conn_data[channel->count2++];
    pnl_base_set_data(PNL_BASE_PTR(conn), cd);
    return pnl_loop_read(loop,conn,echo_read, cd->buffer_a,BUFFERSIZE-1, error);
}


int user_read(pnl_loop_t *l, pnl_connection_t * connection, size_t read_bytes){
    pnl_error_t* error;
    channel_t* channel = pnl_loop_get_data(l);
    conn_data_t* cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    cd->buffer_b[read_bytes] = '\0';
    /*printf("Read: %s\n", cd->buffer_b);*/
    assert_string_equal(cd->buffer_b, messages[cd->message_index_b++]);
    if(strcmp(cd->buffer_b,TERMINATION) == 0){
        pnl_loop_remove_connection(l,connection,error);
        channel->count1 += 1;
        if(channel->count1 == PNL_MAX_CONN) {
            printf("Stopping server...\n");
            pnl_loop_stop(l);
        }
        return PNL_OK;
    }
    size_t numberofbytes  = read_line(cd->buffer_b, BUFFERSIZE,cd->message_index_b);
    return pnl_loop_write(l,connection,user_write, cd->buffer_b, numberofbytes, error);
}

int user_write(pnl_loop_t *l, pnl_connection_t * connection){
    pnl_error_t* error;
    conn_data_t* cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    /*printf("Written everything\n");*/
    return pnl_loop_read(l,connection,user_read, cd->buffer_b,BUFFERSIZE-1, error);

}


int connect_callback (pnl_loop_t *l, pnl_connection_t * connection){
    pnl_error_t* error;
    conn_data_t* cd = pnl_base_get_data(PNL_BASE_PTR(connection));
    size_t numberofbytes  = read_line(cd->buffer_b, BUFFERSIZE,cd->message_index_b);
    return pnl_loop_write(l,connection,user_write,cd->buffer_b, numberofbytes, error);
}


void close_callback(pnl_loop_t * loop, pnl_base_t * base, pnl_error_t* error){

    channel_t* channel = pnl_loop_get_data(loop);
    if(error != NULL){

        /*pnl_print_error(error);*/
        /*pnl_loop_stop(loop);*/

    }else{
        /*puts("Closed gracefully");*/
    }

    if(++channel->close_count == 2*PNL_MAX_CONN){

        pnl_loop_stop(loop);
    }

}

void chat_test(void **state) {
    pnl_loop_t* loop;
    pnl_error_t error;
    channel_t* channel = pnl_malloc(sizeof(channel_t));
    assert_true(channel != NULL);
    int rc;

    pnl_error_reset(&error);

    /*get loop*/
    loop = pnl_get_platform_loop();

    /*initialize loop and set data*/
    pnl_loop_init(loop);
    pnl_loop_set_data(loop,channel);

    rc = pnl_loop_start(loop,start,&error);
    fflush(stdout);
    if(rc == PNL_ERR){
        pnl_print_error(&error);
    }

    for(int i  = 0; i < PNL_MAX_CONN; ++i){
        assert_true(channel->conn_data[i].message_index_a == sizeof(messages)/sizeof(char*));
        assert_true(channel->conn_data[i].message_index_b == sizeof(messages)/sizeof(char*));
    }
    assert_true(rc == PNL_OK);
    pnl_free(channel);
}


int main(int argc, char *argv[]) {

    const struct CMUnitTest tests[] = { cmocka_unit_test(chat_test) };

    return cmocka_run_group_tests(tests,NULL,NULL);

}

