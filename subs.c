/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2009 Thibault Godouet <fcron@free.fr>
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

 /* $Id: subs.c,v 1.29 2008-05-11 11:08:23 thib Exp $ */

#include "global.h"
#include "subs.h"

uid_t
get_user_uid_safe(char *username)
    /* get the uid of user username, and die on error */
{
    struct passwd *pass;

    errno = 0;
    pass = getpwnam(username);
    if ( pass == NULL ) {
	die_e("Unable to get the uid of user %s (is user in passwd file?)",
	      username);
    }
    
    return pass->pw_uid;

}

gid_t
get_group_gid_safe(char *groupname)
    /* get the gid of group groupname, and die on error */
{
    struct group *grp = NULL;

    errno = 0;
    grp = getgrnam(groupname);
    if ( grp == NULL ) {
	die_e("Unable to get the gid of group %s", groupname);
    }
    
    return grp->gr_gid;

}

int
remove_blanks(char *str)
    /* remove blanks at the the end of str */
    /* return the length of the new string */
{
    char *c = str;

    /* scan forward to the null */
    while (*c)
	c++;

    /* scan backward to the first character that is not a space */
    do	{c--;}
    while (c >= str && isspace( (int) *c));

    /* if last char is a '\n', we remove it */
    if ( *c == '\n' )
	*c = '\0';
    else
	/* one character beyond where we stopped above is where the null
	 * goes. */
	*++c = '\0';

    /* return the new length */
    return ( c - str );
    
}

int
strcmp_until(const char *left, const char *right, char until)
/* compare two strings up to a given char (copied from Vixie cron) */
/* Copyright 1988,1990,1993,1994 by Paul Vixie */
/* Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.  */

{
    while (*left != '\0' && *left != until && *left == *right) {
        left++;
        right++;
    }

    if ((*left == '\0' || *left == until)
            && (*right == '\0' || *right == until)) {
        return (0);
    }
    return (*left - *right);
}


char *
strdup2(const char *str)
{
    char *ptr;

    if ( str == NULL )
	return NULL;

    ptr = strdup(str);
    
    if ( ! ptr)
        die_e("Could not strdup()");

    return(ptr);
}

void *
alloc_safe(size_t len, const char * desc)
/* allocate len-bytes of memory, and return the pointer.
 * Die with a log message if there is any error */
{
    void *ptr = NULL;

    ptr = calloc(1, len);
    if ( ptr == NULL ) {
        die_e("Could not allocate %d bytes of memory%s%s", len, (desc)? "for " : "", desc);
    }
    return ptr;
}

void *
realloc_safe(void *cur, size_t len, const char * desc)
/* allocate len-bytes of memory, and return the pointer.
 * Die with a log message if there is any error */
{
    void *new = NULL;

    new = realloc(cur, len);
    if ( new == NULL ) {
        die_e("Could not reallocate %d bytes of memory%s%s", len, (desc)? "for " : "", desc);
    }
    return new;
}

void
free_safe(void *ptr)
    /* free() p and set it to NULL to prevent errors if it is free()ed again */
{
    free(ptr);
    ptr = NULL;
}

int
get_word(char **str)
    /* make str point the next word and return word length */
{
    char *ptr;

    Skip_blanks(*str);
    ptr = *str;

    while ( (isalnum( (int) *ptr) || *ptr == '_' || *ptr == '-') 
	    && *ptr != '=' && ! isspace( (int) *ptr) )
	ptr++;
    
    return (ptr - *str);
}

void
my_unsetenv(const char *name)
/* call unsetenv() if available, otherwise call putenv("var=").
 * Check for errors and log them. */
{

#ifdef HAVE_UNSETENV
    if ( unsetenv(name) < 0 )
        error_e("could not flush env var %s with unsetenv()", name);
#else
    char buf[PATH_LEN];
    snprintf(buf, sizeof(buf) - 1, "%s=", name);
    buf[sizeof(buf)-1] = '\0';
    if ( putenv(buf) < 0 )
        error_e("could not flush env var %s with putenv()", name);
#endif

}

void
my_setenv_overwrite(const char *name, const char *value)
/* call setenv(x, x, 1) if available, otherwise call putenv() with the appropriate
 * constructed string.
 * Check for errors and log them. */

{

#ifdef HAVE_SETENV

    /* // */
    debug("Calling setenv(%s, %s, 1)", name, value);
    /* // */
    if ( setenv(name, value, 1) != 0 )
        error_e("setenv(%s, %s, 1) failed", name, value);

#else
    char buf[PATH_LEN];

    snprintf(buf, sizeof(buf) - 1, "%s=%s", name, value)

    /* The final \0 may not have been copied because of lack of space:
     * add it to make sure */
    buf[sizeof(buf)-1]='\0';

    /* // */
    debug("Calling putenv(%s)", buf);
    /* // */
    if ( putenv(buf) != 0 )
        error_e("putenv(%s) failed", buf);

#endif

}


