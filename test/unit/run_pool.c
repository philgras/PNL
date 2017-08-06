/**
 * Created by pgrassal on 03.08.17.
 */

#include "pnl_thread_pool.h"

#include <stdio.h>
#include <stdlib.h>


int fibo(int number){
    if(number <2){
        return 1;
    }else{
        return fibo(number-1)+fibo(number-2);
    }
}

int test_routine(void * data){

    printf("%d\n",fibo(44));
    return 0;
}



int main(void){


    pnl_thread_pool_t* pool = pnl_thread_pool_create(0);
    if(!pool){
        fprintf(stderr, "Failed to create a thread pool");
        return EXIT_FAILURE;
    }

    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);
    pnl_thread_pool_schedule_job(pool,&test_routine,NULL);

    pnl_thread_pool_finish(pool);
    pnl_thread_pool_free(pool);

    return 0;
}