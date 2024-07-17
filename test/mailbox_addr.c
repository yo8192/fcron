/* Prototype test harness, while waiting for integration of a test framework */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "fcron.h"
#include "job.h"
#include "fcronconf.h"

#define dasserteq(DESCR, RETURNED, EXPECTED) \
{ \
    printf("%s: %ld ?= %ld\n", (DESCR), (long)(RETURNED), (long)(EXPECTED)); \
    assert((RETURNED) == (EXPECTED)); \
}

/* These globals should maybe come from a lib instead of the various exes? */
char *prog_name = NULL;
pid_t daemon_pid = 0;
char foreground = 1;
char default_mail_charset[TERM_LEN] = "";
pam_handle_t *pamh = NULL;
const struct pam_conv apamconv = { NULL };
mode_t saved_umask;
uid_t rootuid = 0;
char *tmp_path = "";

char *format_displayname(char *displayname);
char *make_mailbox_addr(char *displayname, char *mail_from, char *hostname);


void _test_format_displayname(char *test_desc, char *arg, char *expected)
{
    char *output = NULL;

    output = format_displayname(arg);
    printf("%s: format_displayname('%s'): '%s' ?= '%s'\n",
           test_desc, arg, output, expected);
    assert(strcmp(output, expected) == 0);
    Free_safe(output);
}


void _test_make_mailbox_addr(char *test_desc, char *displayname,
                             char *mailfrom, char *hostname, char *expected)
{
    char *output = NULL;

    output = make_mailbox_addr(displayname, mailfrom, hostname);
    printf("%s: make_mailbox_addr('%s', '%s', '%s'): '%s' ?= '%s'\n",
           test_desc, displayname, mailfrom, hostname, output, expected);
    assert(strcmp(output, expected) == 0);
    Free_safe(output);
}

int main(int argc, char* argv[])
{
    char *displayname = NULL;
    char *output = NULL;

    /* Mind that format_displayname() might modify input displayname, thus all
       special characters must be tested */
    _test_format_displayname("empty displayname", "", "");
    _test_format_displayname("displayname with no special char",
                             "Foo Bar", "Foo Bar");
    _test_format_displayname("displayname with no special char",
                             "Foo'Bar", "Foo'Bar");

    /* Make sure to extend this series, should SPECIAL_MBOX_CHARS change. All
       those require quoting */
    _test_format_displayname("displayname with special char",
                             "(Foo Bar", "\"(Foo Bar\"");
    _test_format_displayname("displayname with special char",
                             "Foo Bar)", "\"Foo Bar)\"");
    _test_format_displayname("displayname with special char",
                             "Foo B<ar", "\"Foo B<ar\"");
    _test_format_displayname("displayname with special char",
                             "Fo>o Bar", "\"Fo>o Bar\"");
    _test_format_displayname("displayname with special char",
                             "F[oo Bar", "\"F[oo Bar\"");
    _test_format_displayname("displayname with special char",
                             "Foo] Bar", "\"Foo] Bar\"");
    _test_format_displayname("displayname with special char",
                             "Foo . Bar", "\"Foo . Bar\"");
    _test_format_displayname("displayname with special char",
                             "Foo, Bar", "\"Foo, Bar\"");
    _test_format_displayname("displayname with special char",
                             "Foo B:ar", "\"Foo B:ar\"");
    _test_format_displayname("displayname with special char",
                             "Foo ;Bar", "\"Foo ;Bar\"");
    _test_format_displayname("displayname with special char",
                             "Foo@Bar", "\"Foo@Bar\"");
    _test_format_displayname("displayname with double quote",
                             "Foo \" Bar", "\"Foo \\\" Bar\"");
    _test_format_displayname("displayname with backslash",
                             "Foo \\Bar", "\"Foo \\Bar\"");

    /* make_mailbox_addr() only composes the "mailbox" address header: it's
       agnostic of any special character */
    _test_make_mailbox_addr("empty displayname => no angle brackets",
                            "", "baz", "@quux", "baz@quux");
    _test_make_mailbox_addr("displayname => needs angle brackets",
                            "Foo Bar", "baz", "@quux", "Foo Bar <baz@quux>");


    /* overflow tests */
    displayname = (char *)realloc_safe(displayname,
                                       LINE_LEN + 1 * sizeof(char),
                                       "displayname buffer");
    memset(displayname, 'a', LINE_LEN);
    displayname[LINE_LEN] = '\0';
    displayname[0] = DQUOTE;

    output = format_displayname(displayname);
    dasserteq("format_displayname: overflow", output, NULL);
    Free_safe(output);

    output = make_mailbox_addr(displayname, "baz", "@quux");
    dasserteq("make_mailbox_addr: overflow", output, NULL);
    Free_safe(output);

    Free_safe(displayname);

    return 0;
}

/* Local Variables:  */
/* mode: c           */
/* indent-tabs-mode: nil */
/* End:              */
