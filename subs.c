
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

 /* $Id: subs.c,v 1.14 2001-07-09 11:50:13 thib Exp $ */

#include "global.h"
#include "subs.h"

void init_conf(void);

extern char *tmp_path;
extern char debug_opt;

/* fcron.conf parameters */
char *fcronconf = NULL;
char *fcrontabs = NULL;
char *pidfile = NULL;
char *fcronallow = NULL;
char *fcrondeny = NULL;
char *shell = NULL;
char *sendmail = NULL;
char *editor = NULL;

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
	Set(name_local, tempnam(NULL, NULL));
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


void
init_conf(void)
/* initialises config with compiled in constants */
{
    /* set fcronconf if cmd line option -c has not been used */
    if (fcronconf == NULL)
	fcronconf = strdup2(ETC "/" FCRON_CONF);
    fcrontabs = strdup2(FCRONTABS);
    pidfile = strdup2(PIDFILE);
    fcronallow = strdup2(ETC "/" FCRON_ALLOW);
    fcrondeny = strdup2(ETC "/" FCRON_DENY);
    shell = strdup2(SHELL);
    sendmail = strdup2(SENDMAIL);
    editor = strdup2(EDITOR);
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

    if (fcronconf != NULL)
	/* fcronconf has been set by -c option : file must exist */
	err_on_enoent = 1;
	
    init_conf();

    if ( (f = fopen(fcronconf, "r")) == NULL ) {
	if ( errno == ENOENT ) {
	    if ( err_on_enoent )
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

    /* check if the file is secure : owned and writable only by root */
    if ( fstat(fileno(f), &st) != 0 || st.st_uid != 0
	 || st.st_mode & S_IWGRP || st.st_mode & S_IWOTH ) {
	error("Conf file (%s) must be owned by root and (no more than) 644 : "
	      "ignored", fcronconf);
	fclose(f);
	return;
    }
    
    while ( (ptr1 = fgets(buf, sizeof(buf), f)) != NULL ) {

	Skip_blanks(ptr1); /* at the beginning of the line */
	
	/* ignore comments and blank lines */
	if ( *ptr1 == '#' || *ptr1 == '\n' || *ptr1 == '\0')
	    continue;

	remove_blanks(ptr1); /* at the end of the line */

	ptr2 = ptr1;

	/* get the name of the var */
	while ( (isalnum( (int) *ptr2) || *ptr2 == '_') 
		&& *ptr2 != '=' && ! isspace( (int) *ptr2) )
	    ptr2++;
	
	if ( (namesize = ptr2 - ptr1) == 0 )
	    /* name is zero-length */
	    error("Zero-length var name at line %s : line ignored", buf);

	/* skip the blanks and the "=" and go to the value */
	while ( isspace( (int) *ptr2 ) ) ptr2++;
	if ( *ptr2 == '=' ) ptr2++;
	while ( isspace( (int) *ptr2 ) ) ptr2++;

	/* find which var the line refers to and update it */
	if ( strncmp(ptr1, "fcrontabs", 9) == 0 )
	    fcrontabs = strdup2(ptr2);
	else if ( strncmp(ptr1, "pidfile", 7) == 0 )
	    pidfile = strdup2(ptr2);
	else if ( strncmp(ptr1, "fcronallow", 10) == 0 )
	    fcronallow = strdup2(ptr2);
	else if ( strncmp(ptr1, "fcrondeny", 9) == 0 )
	    fcrondeny = strdup2(ptr2);
	else if ( strncmp(ptr1, "shell", 5) == 0 )
	    shell = strdup2(ptr2);
	else if ( strncmp(ptr1, "sendmail", 8) == 0 )
	    sendmail = strdup2(ptr2);
	else if ( strncmp(ptr1, "editor", 6) == 0 )
	    editor = strdup2(ptr2);
	else
	    error("Unknown var name at line %s : line ignored", buf);

    }

    if (debug_opt) {
	debug("  fcronconf=%s", fcronconf);
/*  	debug("  fcronallow=%s", fcronallow); */
/*  	debug("  fcrondeny=%s", fcrondeny); */
/*  	debug("  fcrontabs=%s", fcrontabs); */
/*  	debug("  pidfile=%s", pidfile); */
/*  	debug("  editor=%s", editor); */
/*  	debug("  shell=%s", shell); */
/*  	debug("  sendmail=%s", sendmail); */
    }

    fclose(f);

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
