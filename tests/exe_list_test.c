#include "../exe_list.h"
#include "../global.h"
#include "../fcron.h"

#include <setjmp.h>             /* setjmp.h is needed by cmocka.h */
#include <cmocka.h>

static void
test_list_init_iterate_destroy(int list_size)
{
    exe_list_t *list = NULL;
    exe_t *e = NULL;
    int i = 0;
    cl_t l1 = {.cl_shell = "line 1" };

    list = exe_list_init();
    assert_non_null(list);

    for (i = 0; i < list_size; i++) {
        exe_list_add_line(list, &l1);
    }

    for (e = exe_list_first(list), i = 0; e != NULL;
         e = exe_list_next(list), i++) {
        assert_string_equal(e->e_line->cl_shell, l1.cl_shell);
    }
    assert_int_equal(i, list_size);

    list = exe_list_destroy(list);
    assert_null(list);
}

static void
test_list_init_iterate_destroy_0(void **state)
{
    test_list_init_iterate_destroy(0);
}

static void
test_list_init_iterate_destroy_1(void **state)
{
    test_list_init_iterate_destroy(1);
}

static void
test_list_init_iterate_destroy_7(void **state)
{
    test_list_init_iterate_destroy(7);
}

static void
test_list_init_iterate_destroy_100(void **state)
{
    test_list_init_iterate_destroy(100);
}

static void
test_list_init_add_remove(void **state)
{
    exe_list_t *list = NULL;
    exe_t *e = NULL;
    int i = 0;
    char *removed_cl_shells[10];
    int removed_cl_shell_count = 0;

    cl_t l1 = {.cl_shell = "line 1" };
    cl_t l2 = {.cl_shell = "line 2" };
    cl_t l3 = {.cl_shell = "line 3" };
    cl_t l4 = {.cl_shell = "line 4" };
    cl_t l5 = {.cl_shell = "line 5" };
    cl_t l6 = {.cl_shell = "line 6" };
    cl_t l7 = {.cl_shell = "line 7" };

    printf("Init the list and populate it...\n");
    list = exe_list_init();
    assert_non_null(list);

    /* trigger a resize of the list during an iteration */
    exe_list_add_line(list, &l1);
    exe_list_add_line(list, &l2);
    exe_list_add_line(list, &l3);
    assert_int_equal(list->num_entries, 3);

    e = exe_list_first(list);
    assert_non_null(e->e_line->cl_shell);
    e = exe_list_next(list);
    assert_non_null(e->e_line->cl_shell);
    exe_list_add_line(list, &l4);
    exe_list_add_line(list, &l5);
    exe_list_add_line(list, &l6);
    exe_list_add_line(list, &l7);
    assert_int_equal(list->num_entries, 7);

    e = exe_list_next(list);
    assert_non_null(e->e_line->cl_shell);
    e = exe_list_next(list);
    assert_non_null(e->e_line->cl_shell);
    e = exe_list_next(list);
    assert_non_null(e->e_line->cl_shell);
    e = exe_list_next(list);
    assert_non_null(e->e_line->cl_shell);
    e = exe_list_next(list);
    assert_non_null(e->e_line->cl_shell);
    /* we reached the end of the list: */
    e = exe_list_next(list);
    assert_null(e);

    /* remove item at the beginning and middle of the list + add an item which is already in there */
    printf("Remove two items...\n");
    e = exe_list_first(list);
    assert_non_null(e->e_line->cl_shell);
    removed_cl_shells[removed_cl_shell_count++] = e->e_line->cl_shell;
    exe_list_remove_cur(list);
    assert_int_equal(list->num_entries, 6);
    e = exe_list_next(list);
    e = exe_list_next(list);
    e = exe_list_next(list);
    removed_cl_shells[removed_cl_shell_count++] = e->e_line->cl_shell;
    exe_list_remove_cur(list);
    assert_int_equal(list->num_entries, 5);
    exe_list_end_iteration(list);
    assert_int_equal(list->num_entries, 5);
    for (e = exe_list_first(list), i = 0; e != NULL;
         e = exe_list_next(list), i++) {
        assert_string_not_equal(e->e_line->cl_shell, removed_cl_shells[0]);
        assert_string_not_equal(e->e_line->cl_shell, removed_cl_shells[1]);
    }
    assert_int_equal(i, 5);
    exe_list_end_iteration(list);
    assert_int_equal(list->num_entries, 5);

    printf("Add a duplicate item...\n");
    e = exe_list_first(list);
    exe_list_add_line(list, e->e_line);
    exe_list_end_iteration(list);
    assert_int_equal(list->num_entries, 6);

    for (e = exe_list_first(list); e != NULL; e = exe_list_next(list)) {
        assert_non_null(e->e_line->cl_shell);
    }

    printf("Remove last item...\n");
    for (i = 0, e = exe_list_first(list); i < list->num_entries - 1;
         i++, e = exe_list_next(list)) ;
    exe_list_remove_cur(list);
    exe_list_end_iteration(list);
    assert_int_equal(list->num_entries, 5);

    printf("Remove an item...\n");
    e = exe_list_first(list);
    e = exe_list_next(list);
    exe_list_remove_cur(list);
    exe_list_end_iteration(list);
    assert_int_equal(list->num_entries, 4);

    for (e = exe_list_first(list); e != NULL; e = exe_list_next(list)) {
        assert_non_null(e->e_line->cl_shell);
    }

    printf("Add an item...\n");
    exe_list_add_line(list, &l1);
    assert_int_equal(list->num_entries, 5);

    for (e = exe_list_first(list); e != NULL; e = exe_list_next(list)) {
        assert_non_null(e->e_line->cl_shell);
    }

    printf("Empty the list...\n");
    e = exe_list_first(list);
    exe_list_remove_cur(list);
    e = exe_list_next(list);
    exe_list_remove_cur(list);
    e = exe_list_next(list);
    exe_list_remove_cur(list);
    e = exe_list_next(list);
    exe_list_remove_cur(list);
    e = exe_list_next(list);
    exe_list_remove_cur(list);
    e = exe_list_next(list);
    assert_int_equal(list->num_entries, 0);

    for (e = exe_list_first(list); e != NULL; e = exe_list_next(list)) {
        assert_non_null(e->e_line->cl_shell);
    }

    printf("Destroying the list...\n");
    list = exe_list_destroy(list);
    assert_null(list);
}

int
main(void)
{
    const struct CMUnitTest mytests[] = {
        cmocka_unit_test(test_list_init_iterate_destroy_0),
        cmocka_unit_test(test_list_init_iterate_destroy_1),
        cmocka_unit_test(test_list_init_iterate_destroy_7),
        cmocka_unit_test(test_list_init_iterate_destroy_100),
        cmocka_unit_test(test_list_init_add_remove),
    };
    return cmocka_run_group_tests(mytests, NULL, NULL);
}
