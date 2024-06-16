#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>             /* setjmp.h is needed by cmocka.h */
#include <cmocka.h>

#include "../fcron.h"
#include "../fcrondyn_svr_priv.h"

ssize_t
__wrap_send(int sockfd, const void *buf, size_t len, int flags)
{
    double up;
    double idle;

    /* Verify the passed value of the argument is correct */
    check_expected_ptr(buf);

    return len;
}

static void
test_print_line(void **state)
{
    int i;
    unsigned char fields[FIELD_NUM_SIZE];

    for (i = 0; i < 1; i++)
        fields[i] = 0;

    bit_set(fields, FIELD_SCHEDULE);
    bit_set(fields, FIELD_USER);

    cf_t cf;
    cl_t cl;
    cl.cl_id = 33;
    cf.cf_user = "myuser";
    cl.cl_file = &cf;
    setenv("TZ", "Europe/London", 1);
    cl.cl_nextexe = 1717337700; /* Sun  2 Jun 15:15:00 BST 2024 */
    time_t until = 1717234200;  /* Sat  1 Jun 10:30:00 BST 2024 */

    cl.cl_shell = "echo blah";
    expect_string(__wrap_send, buf,
                  "33   |myuser   |2024-06-02 15:15:00|echo blah\n");
    print_line(1, &cl, fields, 123, 456, 1717234200);

#define S10 "0123456789"
#define S50 S10 S10 S10 S10 S10
    /* shell command longer than TERM_LEN which will get truncated: */
    cl.cl_shell = "echo " S50 S50 S50 S50 S50;
    char expected_output_truncated[] =
        "33   |myuser   |2024-06-02 15:15:00|echo " S50 S50 S10 S10 S10 S10
        "0 (truncated)\n";
    assert_int_equal(sizeof(expected_output_truncated), TERM_LEN);
    expect_string(__wrap_send, buf, expected_output_truncated);
    print_line(1, &cl, fields, 123, 456, 1717234200);

    /* shell command exactly long enough to use the TERM_LEN length without truncation: */
#define SHELL_CMD "echo " S50 S50 S10 S10 S10 S10 S10 "012"
    cl.cl_shell = SHELL_CMD;
    char expected_output[] =
        "33   |myuser   |2024-06-02 15:15:00|" SHELL_CMD "\n";
    assert_int_equal(sizeof(expected_output), TERM_LEN);
    expect_string(__wrap_send, buf, expected_output);
    print_line(1, &cl, fields, 123, 456, 1717234200);
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_print_line),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
