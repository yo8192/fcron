
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

 /* $Id: subs.c,v 1.9 2001-06-12 06:40:33 thib Exp $ */

#include "global.h"
#include "subs.h"

extern char *tmp_path;

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
temp_file(char **name)
    /* Open a temporary file and return its file descriptor */
{
    int fd;
#ifdef HAVE_MKSTEMP
    char name_local[PATH_LEN] = "";
    strcpy(name_local, tmp_path);
    strcat(name_local, "fcr-XXXXXX");
    if ( (fd = mkstemp(name_local)) == -1 )
	die_e("Can't find a unique temporary filename");
    /* we must set the file mode to 600 (some version of mkstemp may set it
     * incorrectly) */
    if ( fchmod(fd, S_IWUSR | S_IRUSR) != 0 )
	die_e("Can't fchmod temp file");
#else
    const int max_retries = 50;
    char *name_local = NULL;
    int i;

    i = 0;
    do {
	i++;
	free(name_local);
	name_local = tempnam(NULL, NULL);
	if ( name_local == NULL )
	    die("Can't find a unique temporary filename");
	fd = open(name_local, O_RDWR|O_CREAT|O_EXCL|O_APPEND, S_IRUSR|S_IWUSR);
	/* I'm not sure we actually need to be so persistent here */
    } while (fd == -1 && errno == EEXIST && i < max_retries);
    if (fd == -1)
	die_e("Can't open temporary file");
#endif
    if ( name == NULL && unlink(name_local) != 0 )
	die_e("Can't unlink temporary file %s", name_local);

    fcntl(fd, F_SETFD, 1);   /* set close-on-exec flag */
    
    /* give the name of the temp file if necessary */
    if (name != NULL)
	*name = strdup2(name_local);
#ifndef HAVE_MKSTEMP
    free(name_local);
#endif

    return fd;
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
