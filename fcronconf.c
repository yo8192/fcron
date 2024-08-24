/*
 * FCRON - periodic command scheduler
 *
 *  Copyright 2000-2021 Thibault Godouet <fcron@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  The GNU General Public License can also be found in the file
 *  `LICENSE' that comes with the fcron source distribution.
 */


#include "global.h"
#include "mem.h"
#include "fcronconf.h"
#include "subs.h"

void init_conf(void);

extern char debug_opt;
extern uid_t rootuid;
extern gid_t rootgid;

/* fcron.conf parameters */
char *fcronconf = NULL;
char *fcrontabs = NULL;
char *pidfile = NULL;
char *suspendfile = NULL;
char *fifofile = NULL;
char *fcronallow = NULL;
char *fcrondeny = NULL;
char *shell = NULL;
char *sendmail = NULL;
char *editor = NULL;
char *displayname = NULL;

char
*format_displayname(char *conf_value)
    /* Format the input string `conf_value` according to RFC5322 sec. 3.2.3.
     * <https://datatracker.ietf.org/doc/html/rfc5322#section-3.2.3>.
     * Returns: either the formatted displayname (possibly unchanged or empty)
     * as a new dynamically allocated string (must be properly freed by the
     * caller) or NULL on errors like buffer overflow. */
{
    bool need_quotes = false;
    char c = '\0';
    char *bpos = NULL, *dpos = NULL;
    char *buf1 = NULL, *buf2 = NULL;

    /* Shorter than max because of the option prefix "displayname = " */
    const uint buf_len = LINE_LEN - 14;
    uint cwritten = 0;

    if (strlen(conf_value) == 0) return strdup2("");

    buf1 = (char *)alloc_safe(buf_len * sizeof(char), "1st buffer");
    buf2 = (char *)alloc_safe(buf_len * sizeof(char), "2nd buffer");

    /* walk the conf_value and rebuild it in buf1 */
    bpos = buf1;
    for (dpos = conf_value; *dpos; *dpos++) {
        c = *dpos;
        if (strchr(SPECIAL_MBOX_CHARS, c)) {
            /* insert escape */
            if (c == DQUOTE) {
                *bpos++ = BSLASH;
                ++cwritten;
            }
            need_quotes = true;
        }
        if (cwritten >= buf_len) {
            error("Formatted 'displayname' exceeds %u chars", buf_len);
            Free_safe(buf1);
            Free_safe(buf2);
            return NULL;
        }
        *bpos++ = c;
        ++cwritten;
    }

    if (need_quotes) {
        if (snprintf(buf2, buf_len, "\"%s\"", buf1) >= buf_len){
            error("Formatted 'displayname' exceeds %u chars", buf_len);
            Free_safe(buf1);
            Free_safe(buf2);
            return NULL;
        }
        Free_safe(buf1);

        return buf2;
    }

    /* unchanged */
    Free_safe(buf2);

    return buf1;
}

void
init_conf(void)
/* initialises config with compiled in constants */
{
    /* set fcronconf if cmd line option -c has not been used */
    if (fcronconf == NULL)
        fcronconf = strdup2(ETC "/" FCRON_CONF);
    fcrontabs = strdup2(FCRONTABS);
    pidfile = strdup2(PIDFILE);
    suspendfile = strdup2(SUSPEND_FILE);
    fifofile = strdup2(FIFOFILE);
    fcronallow = strdup2(ETC "/" FCRON_ALLOW);
    fcrondeny = strdup2(ETC "/" FCRON_DENY);
    /* // */
    /* shell = strdup2(FCRON_SHELL); */
    shell = strdup2("");

#ifdef SENDMAIL
    sendmail = strdup2(SENDMAIL);
#endif
    editor = strdup2(FCRON_EDITOR);
    displayname = strdup2(DISPLAYNAME);
}

void
free_conf(void)
/* free() the memory allocated in init_conf() */
{
    Free_safe(fcronconf);
    Free_safe(fcrontabs);
    Free_safe(pidfile);
    Free_safe(suspendfile);
    Free_safe(fifofile);
    Free_safe(fcronallow);
    Free_safe(fcrondeny);
    Free_safe(shell);
    Free_safe(sendmail);
    Free_safe(editor);
    Free_safe(displayname);
}

void
read_conf(void)
/* reads in a config file and updates the necessary global variables */
{
    FILE *f = NULL;
    struct stat st;
    char buf[LINE_LEN];
    char *ptr1 = NULL, *ptr2 = NULL;
    short namesize = 0;
    char err_on_enoent = 0;
    struct group *gr = NULL;
    gid_t fcrongid = -1;

    if (fcronconf != NULL)
        /* fcronconf has been set by -c option : file must exist */
        err_on_enoent = 1;

    init_conf();

    if ((f = fopen(fcronconf, "r")) == NULL) {
        if (errno == ENOENT) {
            if (err_on_enoent)
                die_e("Could not read %s", fcronconf);
            else
                /* file does not exist, it is not an error  */
                return;
        }
        else {
            error_e("Could not read %s : config file ignored", fcronconf);
            return;
        }
    }

    /* get fcrongid */
    gr = getgrnam(GROUPNAME);
    if (gr == NULL) {
        die_e("Unable to find %s in /etc/group", GROUPNAME);
    }
    fcrongid = gr->gr_gid;

    /* check if the file is secure : owner:root, group:fcron,
     * writable only by owner */
    if (fstat(fileno(f), &st) != 0
        || st.st_uid != rootuid || st.st_gid != fcrongid
        || st.st_mode & S_IWGRP || st.st_mode & S_IWOTH) {
        error("Conf file (%s) must be owned by root:" GROUPNAME
              " and (no more than) 644 : ignored", fcronconf, GROUPNAME);
        xfclose_check(&f, fcronconf);
        return;
    }

    while ((ptr1 = fgets(buf, sizeof(buf), f)) != NULL) {

        Skip_blanks(ptr1);      /* at the beginning of the line */

        /* ignore comments and blank lines */
        if (*ptr1 == '#' || *ptr1 == '\n' || *ptr1 == '\0')
            continue;

        remove_blanks(ptr1);    /* at the end of the line */

        /* get the name of the var */
        if ((namesize = get_word(&ptr1)) == 0)
            /* name is zero-length */
            error("Zero-length var name at line %s : line ignored", buf);

        ptr2 = ptr1 + namesize;

        /* skip the blanks and the "=" and go to the value */
        while (isspace((int)*ptr2))
            ptr2++;
        if (*ptr2 == '=')
            ptr2++;
        while (isspace((int)*ptr2))
            ptr2++;

        /* find which var the line refers to and update it */
        if (strncmp(ptr1, "fcrontabs", namesize) == 0) {
            Set(fcrontabs, ptr2);
        }
        else if (strncmp(ptr1, "pidfile", namesize) == 0) {
            Set(pidfile, ptr2);
        }
        else if (strncmp(ptr1, "suspendfile", namesize) == 0) {
            Set(suspendfile, ptr2);
        }
        else if (strncmp(ptr1, "fifofile", namesize) == 0) {
            Set(fifofile, ptr2);
        }
        else if (strncmp(ptr1, "fcronallow", namesize) == 0) {
            Set(fcronallow, ptr2);
        }
        else if (strncmp(ptr1, "fcrondeny", namesize) == 0) {
            Set(fcrondeny, ptr2);
        }
        else if (strncmp(ptr1, "shell", namesize) == 0) {
            Set(shell, ptr2);
        }
        else if (strncmp(ptr1, "sendmail", namesize) == 0) {
            Set(sendmail, ptr2);
        }
        else if (strncmp(ptr1, "editor", namesize) == 0) {
            Set(editor, ptr2);
        }
        else if (strncmp(ptr1, "displayname", namesize) == 0) {
            char *output = format_displayname(ptr2);
            Set(displayname, output ? output : "");
            Free_safe(output);
        }
        else
            error("Unknown var name at line %s : line ignored", buf);

    }

    if (debug_opt) {
        debug("  fcronconf=%s", fcronconf);
/*  	debug("  fcronallow=%s", fcronallow); */
/*  	debug("  fcrondeny=%s", fcrondeny); */
/*  	debug("  fcrontabs=%s", fcrontabs); */
/*  	debug("  pidfile=%s", pidfile); */
/*  	debug("  suspendfile=%s", suspendfile); */
/*   	debug("  fifofile=%s", fifofile); */
/*  	debug("  editor=%s", editor); */
/*  	debug("  shell=%s", shell); */
/*  	debug("  sendmail=%s", sendmail); */
    }

    xfclose_check(&f, fcronconf);

}
