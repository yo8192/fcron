
/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2001 Thibault Godouet <fcron@free.fr>
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

 /* $Id: subs.c,v 1.6 2001-04-21 08:55:55 thib Exp $ */

#include "global.h"

extern void die_e(char *fmt, ...);

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
    while (c >= str && isspace(*c));

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


char *
strdup2(const char *str)
{
    char *ptr = malloc(strlen(str) + 1);

    if ( ! ptr)
	die_e("Could not calloc");

    strcpy(ptr, str);
    return(ptr);
}


int
save_type(FILE *f, short int type)
/* save a single type (with no data attached) in a binary fcrontab file */
{
    short int size = 0;

    if ( write(fileno(f), &type, sizeof(type)) < sizeof(type) ) goto err;
    if ( write(fileno(f), &size, sizeof(size)) < sizeof(size) ) goto err;
    
    return OK;

  err:
    return ERR;

}

int
save_str(FILE *f, short int type, char *str)
/* save a string of type "type" in a binary fcrontab file */
{
    short int size = 0;
    size = strlen(str);

    if ( write(fileno(f), &type, sizeof(type)) < sizeof(type) ) goto err;
    if ( write(fileno(f), &size, sizeof(size)) < sizeof(size) ) goto err;
    if ( write(fileno(f), str, size) < size ) goto err;

    return OK;

  err:
    return ERR;
    
}

int
save_strn(FILE *f, short int type, char *str, short int size)
/* save a "size"-length string of type "type" in a binary fcrontab file */
{

    if ( write(fileno(f), &type, sizeof(type)) < sizeof(type) ) goto err;
    if ( write(fileno(f), &size, sizeof(size)) < sizeof(size) ) goto err;
    if ( write(fileno(f), str, size) < size ) goto err;

    return OK;

  err:
    return ERR;
    
}

int
save_lint(FILE *f, short int type, long int value)
/* save an integer of type "type" in a binary fcrontab file */
{
    short int size = sizeof(value);

    if ( write(fileno(f), &type, sizeof(type)) < sizeof(type) ) goto err;
    if ( write(fileno(f), &size, sizeof(size)) < sizeof(size) ) goto err;
    if ( write(fileno(f), &value, size) < size ) goto err;

    return OK;

  err:
    return ERR;
    
}
