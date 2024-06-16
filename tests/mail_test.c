/* Prototype test harness, while waiting for integration of a test framework */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>             /* setjmp.h is needed by cmocka.h */
#include <cmocka.h>

#include "../fcron.h"
#include "../job.h"
#include "../fcronconf.h"
#include "../mail.h"

#define TestFormatDisplayname(DESC, ARG, EXPECTED) \
{ \
    char *_local_output = NULL; \
    _local_output = format_maildisplayname((ARG)); \
    assert_string_equal(_local_output, (EXPECTED)); \
    Free_safe(_local_output); \
}

#define TestMakeMailboxAddr(DESC, DISPLAYNAME, MAILFROM, HOSTNAME, EXPECTED) \
{ \
    char *_local_output = NULL; \
    _local_output = make_mailbox_addr((DISPLAYNAME), (MAILFROM), (HOSTNAME)); \
    assert_string_equal(_local_output, (EXPECTED)); \
    Free_safe(_local_output); \
}

static void
test_format_displayname_no_special_char(void **state)
{
    /* Mind that format_maildisplayname() might modify input displayname, thus all
     * special characters must be tested */
    TestFormatDisplayname("empty displayname", "", "");
    TestFormatDisplayname("displayname with no special char",
                          "Foo Bar", "Foo Bar");
    TestFormatDisplayname("displayname with no special char",
                          "Foo'Bar", "Foo'Bar");
}

static void
test_format_displayname_quoted_special_char(void **state)
{
    /* Make sure to extend this series, should SPECIAL_MBOX_CHARS change. All
     * those require quoting */
    TestFormatDisplayname("displayname with special char",
                          "(Foo Bar", "\"(Foo Bar\"");
    TestFormatDisplayname("displayname with special char",
                          "Foo Bar)", "\"Foo Bar)\"");
    TestFormatDisplayname("displayname with special char",
                          "Foo B<ar", "\"Foo B<ar\"");
    TestFormatDisplayname("displayname with special char",
                          "Fo>o Bar", "\"Fo>o Bar\"");
    TestFormatDisplayname("displayname with special char",
                          "F[oo Bar", "\"F[oo Bar\"");
    TestFormatDisplayname("displayname with special char",
                          "Foo] Bar", "\"Foo] Bar\"");
    TestFormatDisplayname("displayname with special char",
                          "Foo . Bar", "\"Foo . Bar\"");
    TestFormatDisplayname("displayname with special char",
                          "Foo, Bar", "\"Foo, Bar\"");
    TestFormatDisplayname("displayname with special char",
                          "Foo B:ar", "\"Foo B:ar\"");
    TestFormatDisplayname("displayname with special char",
                          "Foo ;Bar", "\"Foo ;Bar\"");
    TestFormatDisplayname("displayname with special char",
                          "Foo@Bar", "\"Foo@Bar\"");
    TestFormatDisplayname("displayname with double quote",
                          "Foo \" Bar", "\"Foo \\\" Bar\"");
    TestFormatDisplayname("displayname with backslash",
                          "Foo \\Bar", "\"Foo \\Bar\"");
}

static void
test_format_displayname_overflow_no_expansion(void **state)
{
    char *displayname = NULL;
    char *output = NULL;

    displayname = (char *)realloc_safe(displayname,
                                       MAIL_FROM_VALUE_LEN_MAX * 2 *
                                       sizeof(char), "displayname buffer");

    memset(displayname, 'a', MAIL_FROM_VALUE_LEN_MAX * 2);

    displayname[MAIL_FROM_VALUE_LEN_MAX + 1] = '\0';
    output = format_maildisplayname(displayname);
    assert_null(output);

    Free_safe(output);
    Free_safe(displayname);
}

static void
test_format_displayname_max_size_no_special_char(void **state)
{
    char *displayname = NULL;
    char *output = NULL;

    displayname = (char *)realloc_safe(displayname,
                                       MAIL_FROM_VALUE_LEN_MAX * 2 *
                                       sizeof(char), "displayname buffer");

    memset(displayname, 'a', MAIL_FROM_VALUE_LEN_MAX * 2);

    displayname[MAIL_FROM_VALUE_LEN_MAX] = '\0';
    output = format_maildisplayname(displayname);
    TestFormatDisplayname("format_displayname: max size with no special chars",
                          output, displayname);
    assert_int_equal(strlen(output), MAIL_FROM_VALUE_LEN_MAX);

    Free_safe(output);
    Free_safe(displayname);
}


static void
test_format_displayname_max_size_special_char_overflow(void **state)
{
    char *displayname = NULL;
    char *output = NULL;

    displayname = (char *)realloc_safe(displayname,
                                       MAIL_FROM_VALUE_LEN_MAX * 2 *
                                       sizeof(char), "displayname buffer");

    memset(displayname, 'a', MAIL_FROM_VALUE_LEN_MAX * 2);

    displayname[0] = DQUOTE;
    output = format_maildisplayname(displayname);
    assert_null(output);        /* format_displayname: overflow on max size with special char */
    Free_safe(output);
    Free_safe(displayname);
}

static void
test_make_mailbox_addr(void **state)
{
    /* make_mailbox_addr() only composes the "mailbox" address header: it's
     * agnostic of any special character */
    TestMakeMailboxAddr("empty displayname => no angle brackets",
                        "", "baz", "@quux", "baz@quux");
    TestMakeMailboxAddr("displayname => needs angle brackets",
                        "Foo Bar", "baz", "@quux", "Foo Bar <baz@quux>");

}

static void
test_make_mailbox_addr_max_size(void **state)
{
    char *displayname = NULL;
    char *output = NULL;

    displayname = (char *)realloc_safe(displayname,
                                       MAIL_FROM_VALUE_LEN_MAX * 2 *
                                       sizeof(char), "displayname buffer");

    memset(displayname, 'a', MAIL_FROM_VALUE_LEN_MAX * 2);

    /* make_mailbox_addr() adds 3 chars: space, <, > */
    displayname[MAIL_FROM_VALUE_LEN_MAX - strlen("baz") - strlen("@quux") - 3] =
        '\0';
    output = make_mailbox_addr(displayname, "baz", "@quux");
    assert_int_equal(strlen(output), MAIL_FROM_VALUE_LEN_MAX);
    assert_non_null(output);    /* make_mailbox_addr: max size */

    Free_safe(output);
    Free_safe(displayname);
}

static void
test_make_mailbox_addr_overflow(void **state)
{
    char *displayname = NULL;
    char *output = NULL;

    displayname = (char *)realloc_safe(displayname,
                                       MAIL_FROM_VALUE_LEN_MAX * 2 *
                                       sizeof(char), "displayname buffer");

    memset(displayname, 'a', MAIL_FROM_VALUE_LEN_MAX * 2);

    /* make_mailbox_addr() adds 3 chars: space, <, > */
    displayname[MAIL_FROM_VALUE_LEN_MAX - strlen("baz") - strlen("@quux") - 2] =
        '\0';
    output = make_mailbox_addr(displayname, "baz", "@quux");
    assert_null(output);        /* make_mailbox_addr: overflow */

    Free_safe(output);
    Free_safe(displayname);
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_format_displayname_no_special_char),
        cmocka_unit_test(test_format_displayname_quoted_special_char),
        cmocka_unit_test(test_format_displayname_overflow_no_expansion),
        cmocka_unit_test(test_format_displayname_max_size_no_special_char),
        cmocka_unit_test
            (test_format_displayname_max_size_special_char_overflow),
        cmocka_unit_test(test_make_mailbox_addr),
        cmocka_unit_test(test_make_mailbox_addr_max_size),
        cmocka_unit_test(test_make_mailbox_addr_overflow),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

/* Local Variables:  */
/* mode: c           */
/* indent-tabs-mode: nil */
/* End:              */
