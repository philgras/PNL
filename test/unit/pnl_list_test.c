#include "pnl_list.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define ASSERT_EQ(v1,v2) if(v1 != v2) exit(EXIT_FAILURE)
struct loop{
    pnl_list_t conns;
};

struct connection{

    int i;
    void* data;
    pnl_list_t list;
    
};


int find(pnl_list_t* l, int i);
size_t getSize(pnl_list_t* l);

static inline void init_connection(struct connection* conn, int i){
    conn->i = i;
    conn->data = NULL;
    pnl_list_init(&(conn->list));
}


int find(pnl_list_t* l, int i){
    int rv = -1;
    PNL_LIST_FOR_EACH(l,iter){
        if(PNL_LIST_ENTRY(iter,struct connection, list)->i == i)
            rv = 0;
    }
    
    return rv;
}

size_t getSize(pnl_list_t* l){
    size_t rv = 0;
    PNL_LIST_FOR_EACH(l,iter){
        ++rv;
    }
    
    return rv;
}

int main(void){
    
    int cnt = 0;

    struct loop l;
    struct connection c1,c2,c3,c4,c5;
    init_connection(&c1,1);
    init_connection(&c2,2);
    init_connection(&c3,3);
    init_connection(&c4,4);
    init_connection(&c5,5);
    
    
    pnl_list_init(&(l.conns));
    pnl_list_insert(&(l.conns),&(c1.list));
    pnl_list_insert(&(l.conns),&(c2.list));
    pnl_list_insert(&(l.conns),&(c3.list));
    pnl_list_insert(&(l.conns),&(c4.list));
    pnl_list_insert(&(l.conns),&(c5.list));
       
    ASSERT_EQ(0,find(&(l.conns),1));
    ASSERT_EQ(0,find(&(l.conns),2));
    ASSERT_EQ(0,find(&(l.conns),3));
    ASSERT_EQ(0,find(&(l.conns),4));
    ASSERT_EQ(0,find(&(l.conns),5));
    
    
    pnl_list_remove(&(c3.list));
    
    ASSERT_EQ(-1,find(&(l.conns),3));
    ASSERT_EQ(4,getSize(&(l.conns)));
    
    while(!pnl_list_is_empty(&l.conns)){
        pnl_list_remove(pnl_list_first(&l.conns));
        ++cnt;
    }
    
    ASSERT_EQ(cnt,4);
    
    return EXIT_SUCCESS;
}
