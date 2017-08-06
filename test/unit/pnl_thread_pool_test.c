/**
 * Created by pgrassal on 28.07.17.
 */

#include "pnl_thread_pool.h"
#include "pnl_thread_pool.c"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <pthread.h>


void create_pool_test(void **state) {
    pnl_thread_pool_t* pool = pnl_thread_pool_create(4);
    assert_true(pool != NULL);
    assert_true(pnl_list_is_empty(&pool->job_list) == 0);
    assert_true(pool->number_of_threads == 4);
}

void add_job_successfully_test(void **state) {

}

void add_job_failure_test(void **state){

}


void free_pool_test(void **state){

}


int main(int argc, char *argv[]) {

    const struct CMUnitTest tests[] = {cmocka_unit_test(create_pool_test),
                                       cmocka_unit_test(add_job_successfully_test),
                                       cmocka_unit_test(add_job_failure_test),
                                       cmocka_unit_test(free_pool_test)};

    return cmocka_run_group_tests(tests, NULL, NULL);

}
