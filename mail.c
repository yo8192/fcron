/*
 * FCRON - periodic command scheduler
 *
 *  Copyright 2000-2025 Thibault Godouet <fcron@free.fr>
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


#include "fcron.h"
#include "mail.h"

char *
format_maildisplayname(char *displayname_conf)
    /* Format the input string `conf_value` according to RFC5322 sec. 3.2.3.
     * <https://datatracker.ietf.org/doc/html/rfc5322#section-3.2.3>.
     * Returns: either the formatted displayname (possibly unchanged or empty)
     * as a new dynamically allocated string (must be properly freed by the
     * caller) or NULL on errors like buffer overflow. */
{
    bool need_quotes = false;
    char c = '\0';
    char *ipos = NULL;          /* Input position */
    char *output = NULL, *quoted_output = NULL;

    const uint buf_len = MAIL_FROM_VALUE_LEN_MAX;
    uint cwritten = 0;          /* how many chars we have written */

    if (strlen(displayname_conf) == 0) {
        return strdup2("");
    }

    output = (char *)alloc_safe(buf_len * sizeof(char), "output buffer");

    /* walk the conf_value and rebuild it in buf1 */
    for (ipos = displayname_conf; *ipos; ipos++) {
        c = *ipos;
        if (strchr(SPECIAL_MBOX_CHARS, c)) {
            /* insert escape */
            if (c == DQUOTE) {
                output[cwritten] = BSLASH;
                cwritten++;
            }
            need_quotes = true;
        }
        if (cwritten >= buf_len) {
            error("Formatted 'displayname' exceeds %u chars", buf_len);
            Free_safe(output);
            return NULL;
        }
        output[cwritten] = c;
        cwritten++;
    }

    if (need_quotes) {
        quoted_output =
            (char *)alloc_safe(buf_len * sizeof(char), "quoted output buffer");
        int needed_len = snprintf(quoted_output, buf_len, "\"%s\"", output);
        if (needed_len >= buf_len) {
            error("Formatted 'displayname' too long: length:%u > max:%u chars",
                  needed_len, buf_len);
            Free_safe(output);
            Free_safe(quoted_output);
            return NULL;
        }
        Free_safe(output);

        return quoted_output;
    }

    return output;
}

char *
make_mailbox_addr(char *displayname_conf, char *mail_from, char *hostname)
    /* Produce a "mailbox" header as per RFC5322 sec. 3.2.3
     * <https://datatracker.ietf.org/doc/html/rfc5322#section-3.2.3>.
     * Returns: either the formatted mailbox header as a new dynamically
     * allocated string (must be properly freed by the caller) or NULL on
     * errors like buffer overflow. */
{
    char *buf = NULL;
    uint written = 0;
    bool need_anglebrackets = false;

    const uint buf_len = MAIL_FROM_VALUE_LEN_MAX + 1;

    buf = (char *)alloc_safe(buf_len * sizeof(char), "mailbox addr buffer");

    if (displayname_conf[0] != '\0') {
        /* displayname_conf isn't an empty string */
        need_anglebrackets = true;
    }

    /* no @ here, it's handled upstream */
    if (need_anglebrackets)
        written = snprintf(buf, buf_len, "%s %c%s%s%c",
                           displayname_conf, '<', mail_from, hostname, '>');
    else
        written = snprintf(buf, buf_len, "%s%s", mail_from, hostname);

    if (written >= buf_len) {
        error("Mailbox addr exceeds %u chars", buf_len);
        Free_safe(buf);
        return NULL;
    }

    return buf;
}
