#ifndef PNL_LOOP_H
#define PNL_LOOP_H

#include "pnl_list.h"
#define INVALID_FD -1;

typedef int pnl_fd_t;


struct pnl_tcpconn_s;

typedef void (*on_close_cb)(pnl_fd_t*);
typedef void (*on_accept_cb)(pnl_tcpconn_s*);
typedef void (*on_recv_cb)(pnl_tcpconn_s*);
typedef void (*on_send_cb)(pnl_tcpconn_s*);

typedef struct{
    /*list interface*/
    pnl_list node;    
    /*the server socket that is listening on a certain port*/
    pnl_fd_t socket_fd;
    /*callback is called upon closing the server socket*/
    on_close_cb close_cb;    
}pnl_tcpserver_t;

typedef struct pnl_tcpconn_s{
    /*list interface*/
    pnl_list node;
    /*the stream socket that can be used for reading and writing*/
    pnl_fd_t socket_fd;
    /*callback after accept*/
    on_accept_cb accept_cb;
    /*callback after the recv operation is finished succefully*/
    on_recv_cb read_cb;
    /*callback after the write operation is finished succefully*/
    on_write_cb write_cb;
    /*callback is called upon closing the socket*/
    on_close_cb close_cb; 
};

typedef struct pnl_tcpconn_s pnl_tcpconn_t;

typedef struct{
    pnl_fd_t epollfd;

    pnl_list active_fds;
    
}pnl_loop_t;


pnl_loop_t* get_platform_loop(void);

void pnl_loop_init(pnl_loop_t*);
void pnl_loop_start(pnl_loop_t*);
void pnl_loop_stop(pnl_loop_t*);


#endif /*PNL_LOOP_H*/
