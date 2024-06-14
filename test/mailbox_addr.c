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
    assert((RETURNED) == (EXPECTED));                      \
}

#define dassertcmp(DESCR, RETURNED, EXPECTED) \
{ \
    printf("%s: %s ?= %s\n", (DESCR), (RETURNED), (EXPECTED));   \
    assert(!strcmp((RETURNED), (EXPECTED)));                      \
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


void _test(char *pdescr, char *displayname, char *mailfrom, char *hostname)
{
    char descr[2*LINE_LEN] = "";
    char expected[2*LINE_LEN] = "";
    char *qdisplayname = NULL, *mailbox=NULL;

    qdisplayname = format_displayname(displayname);
    sprintf(descr, "format_displayname: %s", pdescr);
    dassertcmp(descr, qdisplayname, displayname);

    if (strlen(displayname))
        snprintf(expected, sizeof(expected), "%s <%s%s>",
                 qdisplayname, mailfrom, hostname);
    else
        snprintf(expected, sizeof(expected), "%s%s", mailfrom, hostname);

    mailbox = make_mailbox_addr(qdisplayname, mailfrom, hostname);
    sprintf(descr, "make_mailbox_addr: %s", pdescr);
    dassertcmp(descr, mailbox, expected);

    Free_safe(qdisplayname);
    Free_safe(mailbox);
}


void _test_c(char *pdescr, char *mailfrom, char *hostname, char c)
{
    char descr[2*LINE_LEN];
    char expected[2*LINE_LEN];
    char displayname[LINE_LEN];
    char *qdisplayname = NULL, *mailbox=NULL;

    snprintf(displayname, sizeof(displayname), "Foo %c Bar", c);
    if (c == DQUOTE)
        snprintf(expected, sizeof(expected), "\"Foo %c%c Bar\"", BSLASH, c);
    else
        snprintf(expected, sizeof(expected), "\"%s\"", displayname);
    qdisplayname = format_displayname(displayname);
    sprintf(descr, "format_displayname: %s '%c'", pdescr, c);
    dassertcmp(descr, qdisplayname, expected);

    snprintf(expected, sizeof(expected), "%s <%s%s>", qdisplayname, mailfrom, hostname);
    mailbox = make_mailbox_addr(qdisplayname, mailfrom, hostname);
    sprintf(descr, "make_mailbox_addr: %s '%c'", pdescr, c);
    dassertcmp(descr, mailbox, expected);

    Free_safe(qdisplayname);
    Free_safe(mailbox);
}

int main(int argc, char* argv[])
{
    char *displayname = NULL;
    char *qdisplayname = NULL;
    char *mailfrom=NULL;
    char *hostname=NULL;
    char *mailbox=NULL, *walker=NULL;
    char c = '\0';

    mailfrom = strdup2("baz");
    hostname = strdup2("@quux");

    displayname = strdup2("");
    _test("empty displayname", displayname, mailfrom, hostname);

    displayname = strdup2("Foo Bar");
    _test("displayname with no special char", displayname, mailfrom, hostname);

    walker = SPECIAL_MBOX_CHARS;
    while (*walker) {
        c = *walker++;
        _test_c("displayname with special char", mailfrom, hostname, c);
	}

    /* overflow tests */
    displayname = (char *)realloc_safe(displayname,
                                       LINE_LEN + 1 * sizeof(char),
                                       "displayname buffer");
    memset(displayname, 'a', LINE_LEN);
    displayname[LINE_LEN] = '\0';
    displayname[0] = DQUOTE;

    qdisplayname = format_displayname(displayname);
    dasserteq("format_displayname: overflow", qdisplayname, NULL);

    mailbox = make_mailbox_addr(displayname, mailfrom, hostname);
    dasserteq("make_mailbox_addr: overflow", mailbox, NULL);

    Free_safe(mailbox);
    Free_safe(hostname);
    Free_safe(mailfrom);
    Free_safe(displayname);
    Free_safe(qdisplayname);

    return 0;
}

/* Local Variables:  */
/* mode: c           */
/* indent-tabs-mode: nil */
/* End:              */
