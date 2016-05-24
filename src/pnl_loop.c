#include "pnl_loop.h"
#include "pnl.h"
#include <sys/epoll.h>


static pnl_loop unix_loop;

pnl_loop_t* get_platform_loop(void){

    return unix_loop;

}

void pnl_loop_init(pnl_loop_t* l){

    l->epollfd  = INVALID_FD;
    pnl_list_init(l->active_fds);

}

void pnl_loop_start(pnl_loop_t*){
    //initilize epollfd
    while(true){
    
    
    
    }
}

void pnl_loop_stop(pnl_loop_t*){

}
