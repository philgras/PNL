#include "pnl_buffer.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#define PNL_TEST_BUFFER_SIZE 4

void buffer_test(void **state) {
    pnl_buffer_t buffer;
    char data[PNL_TEST_BUFFER_SIZE]  = {1,2,3,4};

    pnl_buffer_init(&buffer);
    assert_true(buffer.data == NULL);
    assert_true(buffer.position == NULL);
    assert_true(buffer.used == 0);
    assert_true(buffer.size == 0);

    pnl_buffer_set_data(&buffer,data, PNL_TEST_BUFFER_SIZE);
    assert_true(buffer.data == data);
    assert_true(buffer.position == data);
    assert_true(buffer.used == 0);
    assert_true(buffer.size == PNL_TEST_BUFFER_SIZE);

    /*
     * Add some more test logic...
     */

}


int main(int argc, char *argv[]) {

    const struct CMUnitTest tests[] = {cmocka_unit_test(buffer_test)};

    return cmocka_run_group_tests(tests, NULL, NULL);

}

