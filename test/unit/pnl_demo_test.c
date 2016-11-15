/*
 * pnl_demo_test.c
 *
 *  Created on: 29.07.2016
 *      Author: philgras
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

void demo_test(void **state) {

}


int main(int argc, char *argv[]) {

    const struct CMUnitTest tests[] = { cmocka_unit_test(demo_test) };

    return cmocka_run_group_tests(tests,NULL,NULL);

}

