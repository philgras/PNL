#include "pnl_list.h"


#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#define NUMBER_LIST_LENGTH 5

typedef struct {

    pnl_list_t node;
    int number;

} number_node_t;

void test_insert_and_remove(void **state) {
    pnl_list_t number_list, *element;
    number_node_t nodes[NUMBER_LIST_LENGTH];

    pnl_list_init(&number_list);
    assert_true(number_list.next == number_list.prev);
    assert_true(pnl_list_is_empty(&number_list));
    assert_true(pnl_list_first(&number_list) == NULL);

    for (int t = 0; t < NUMBER_LIST_LENGTH; ++t) {
        pnl_list_init(&nodes[t].node);
        nodes[t].number = t;
        pnl_list_insert(&number_list, &nodes[t].node);

        assert_true(number_list.next == &nodes[t].node);
        assert_true(&number_list == nodes[t].node.prev);
        assert_false(pnl_list_is_empty(&number_list));

        element = pnl_list_first(&number_list);
        assert_true(element == &nodes[t].node);
        assert_true(PNL_LIST_ENTRY(element, number_node_t, node)->number == t);
        assert_true(number_list.prev == &nodes[0].node);

    }

    for (int t = NUMBER_LIST_LENGTH-1; t >= 0; --t) {
        element = nodes[t].node.next;
        pnl_list_remove(&nodes[t].node);
        assert_true(number_list.next == element);
    }

    assert_true(pnl_list_is_empty(&number_list));
    assert_true(pnl_list_first(&number_list) == NULL);
}

void test_for_each(void **state) {
    pnl_list_t number_list;
    number_node_t nodes[NUMBER_LIST_LENGTH];

    pnl_list_init(&number_list);

    for (int t = 0; t < NUMBER_LIST_LENGTH; ++t) {
        pnl_list_init(&nodes[t].node);
        nodes[t].number = t;
        pnl_list_insert(&number_list, &nodes[t].node);
    }


    PNL_LIST_FOR_EACH(&number_list,iter){
        pnl_list_remove(iter);
    }

    assert_true(pnl_list_is_empty(&number_list));
}

int main(int argc, char *argv[]) {

    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_insert_and_remove),
            cmocka_unit_test(test_for_each)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);

}
