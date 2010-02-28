/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2010 Thibault Godouet <fcron@free.fr>
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

void init_conf(void);

extern char debug_opt;
extern uid_t rootuid;
extern gid_t rootgid;

/* fcron.conf parameters */
char *fcronconf = NULL;
char *fcrontabs = NULL;
char *pidfile = NULL;
char *fifofile = NULL;
char *fcronallow = NULL;
char *fcrondeny = NULL;
char *shell = NULL;
char *sendmail = NULL;
char *editor = NULL;


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

#ifdef USE_SETE_ID
void
seteuid_safe(uid_t euid)
/* set the euid if different from the current one, and die on error */
{
    /* on BSD, one can only seteuid() to the real UID or saved UID,
     * and NOT the euid, so we don't call seteuid(geteuid()),
     * which is why we need to check if a change is needed */

    if (geteuid() != euid && seteuid(euid) != 0)
        die_e("could not change euid to %d", euid);

}

void
setegid_safe(gid_t egid)
/* set the egid if different from the current one, and die on error */
{
    /* on BSD, one can only setegid() to the real GID or saved GID,
     * and NOT the egid, so we don't call setegid(getegid()),
     * which is why we need to check if a change is needed */

    if (getegid() != egid && setegid(egid) != 0)
        die_e("could not change egid to %d", egid);

}
#endif /* def USE_SETE_ID */

#ifdef USE_SETE_ID
int
open_as_user(const char *pathname, uid_t openuid, gid_t opengid, int flags, ...)
/* Become user and call open(), then revert back to who we were.
 * NOTE: when flags & O_CREAT, the 5th argument is mode_t and must be set
 *       -- it is ignored otherwise */
{
    uid_t orig_euid = geteuid();
    gid_t orig_egid = getegid();
    struct stat s;
    int fd = -1;
    va_list ap;
    mode_t mode = (mode_t) 0;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    seteuid_safe(openuid);
    setegid_safe(opengid);

    if (flags & O_CREAT) {
        fd = open(pathname, flags, mode);
    }
    else
        fd = open(pathname, flags);

    /* change the effective uid/gid back to original values */
    seteuid_safe(orig_euid);
    setegid_safe(orig_egid);

    /* if open() didn't fail make sure we opened a 'normal' file */
    if ( fd >= 0 ) {

        if ( fstat(fd, &s) < 0 ) {
            error_e("open_as_user(): could not fstat %s", pathname);
            if ( close(fd) < 0 )
                error_e("open_as_user: could not close() %s", pathname);
            fd = -1;
        }

        if ( ! S_ISREG(s.st_mode) || s.st_nlink != 1 ) {
            error_e("open_as_user(): file %s is not a regular file", pathname);
            if ( close(fd) < 0 )
                error_e("open_as_user: could not close() %s", pathname);
            errno = 0;
            fd = -1;
        }

    }

    return fd;

}

#else /* def USE_SETE_ID */

int
open_as_user(const char *pathname, uid_t openuid, gid_t opengid, int flags, ...)
/* Become user and call open(), then revert back to who we were.
 * As seteuid() is not available on this system attempt to similate that behavior
 * as closely as possible.
 * NOTE: when flags & O_CREAT, the 5th argument is mode_t and must be set
 *       -- it is ignored otherwise */
{
    int fd = -1;
    struct stat s;
    va_list ap;
    mode_t mode = (mode_t) 0;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    /* In case a flag as O_TRUNC is set, we should test if the user
     * is allowed to open the file before we open it.
     * There will always be a risk of race-condition between the test
     * and the open but that's the best we can realistically do
     * without seteuid()... */
    if ( stat(pathname, &s) == 0 ) {
        if ( ! ( s.st_mode & S_IROTH
                    || ( s.st_uid == openuid && s.st_mode & S_IRUSR )
                    || ( s.st_gid == opengid && s.st_mode & S_IRGRP ) ) ) {
            error("open_as_user(): file %s does not pass the security test: "
                    "uid=%d gid=%d mode=%lo openuid=%d opengid=%d",
                    pathname, s.st_uid, s.st_gid, s.st_mode, openuid, opengid);
            errno = EACCES;
            return -1;
        }
    }
    else if ( errno == ENOENT ) {
        /* the file doesn't exist so no risk to truncate the wrong file! */
        ;
    }
    else {
        error_e("open_as_user(): could not stat %s", pathname);
        return -1;
    }

    if (flags & O_CREAT) {
        fd = open(pathname, flags, mode);
    }
    else
        fd = open(pathname, flags);

    if ( fd < 0 )
        /* we couldn't open the file */
        return fd;

    /* if open() didn't fail make sure we opened a 'normal' file */
    if ( fstat(fd, &s) < 0 ) {
        error_e("open_as_user(): could not fstat %s", pathname);
        goto err;
    }
    if ( ! S_ISREG(s.st_mode) || s.st_nlink != 1 ) {
        error_e("open_as_user(): file %s is not a regular file", pathname);
        goto err;
    }

    /* we couldn't become openuid/opengid, so check manually if the user
     * is allowed to read that file
     * We do that again as a malicious user could have replaced the file
     * by another one (e.g. a link) between the stat() and the open() earlier */
    if ( ! ( s.st_mode & S_IROTH
            || ( s.st_uid == openuid && s.st_mode & S_IRUSR )
            || ( s.st_gid == opengid && s.st_mode & S_IRGRP ) ) ) {
        error("open_as_user(): file %s does not pass the security test: "
                "uid=%d gid=%d mode=%lo openuid=%d opengid=%d",
                pathname, s.st_uid, s.st_gid, s.st_mode, openuid, opengid);
        errno = EACCES;
        goto err;
    }

    /* if we created a new file, change the file ownership:
     * make it as it would be if we had seteuid() 
     * NOTE: if O_CREAT was set without O_EXCL and the file existed before
     *       then we will end up changing the ownership even if the seteuid()
     *       version of that function wouldn't have. That shouldn't break
     *       anything though. */
    if ( (flags & O_CREAT) && fchown(fd, openuid, opengid) != 0) {
        error_e("Could not fchown %s to uid:%d gid:%d", pathname, openuid, opengid);
        if ( close(fd) < 0 )
            error_e("open_as_user: could not close() %s", pathname);
        return -1;
    }

    /* everything went ok: return the file descriptor */
    return fd;

err:
    if ( fd >= 0 && close(fd) < 0 )
        error_e("open_as_user: could not close() %s", pathname);
    return -1;
}

#endif /* def USE_SETE_ID */

int
remove_as_user(const char *pathname, uid_t removeuid, gid_t removegid)
/* Become user and call remove(), then revert back to who we were */
{
    int rval = -1;
#ifdef USE_SETE_ID
    uid_t orig_euid = geteuid();
    gid_t orig_egid = getegid();

    seteuid_safe(removeuid);
    setegid_safe(removegid);
#endif /* def USE_SETE_ID */

    rval = remove(pathname);

#ifdef USE_SETE_ID
    seteuid_safe(orig_euid);
    setegid_safe(orig_egid);
#endif /* def USE_SETE_ID */

    return rval;
}

int
rename_as_user(const char *oldpath, const char *newpath, uid_t renameuid, gid_t renamegid)
/* Become user and call rename(), then revert back to who we were */
{
    int rval = -1;
#ifdef USE_SETE_ID
    uid_t orig_euid = geteuid();
    gid_t orig_egid = getegid();

    seteuid_safe(renameuid);
    setegid_safe(renamegid);
#endif /* def USE_SETE_ID */

    rval = rename(oldpath, newpath);

#ifdef USE_SETE_ID
    seteuid_safe(orig_euid);
    setegid_safe(orig_egid);
#endif /* def USE_SETE_ID */

    return rval;

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


char *
strdup2(const char *str)
{
    char *ptr;

    if ( str == NULL )
	return NULL;

    ptr = malloc(strlen(str) + 1);
    
    if ( ! ptr)
	die_e("Could not calloc");

    strcpy(ptr, str);
    return(ptr);
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
init_conf(void)
/* initialises config with compiled in constants */
{
    /* set fcronconf if cmd line option -c has not been used */
    if (fcronconf == NULL)
	fcronconf = strdup2(ETC "/" FCRON_CONF);
    fcrontabs = strdup2(FCRONTABS);
    pidfile = strdup2(PIDFILE);
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
}

void
free_conf(void)
/* free() the memory allocated in init_conf() */
{
    free(fcronconf);
    free(fcrontabs);
    free(pidfile);
    free(fifofile);
    free(fcronallow);
    free(fcrondeny);
    free(shell);
    free(sendmail);
    free(editor);
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

    f = fopen(fcronconf, "r");
    if ( f == NULL ) {
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

    /* get fcrongid */
    gr = getgrnam(GROUPNAME);
    if ( gr == NULL ) {
	die_e("Unable to find %s in /etc/group", GROUPNAME);
    }
    fcrongid = gr->gr_gid;

    /* check if the file is secure : owner:root, group:fcron,
     * writable only by owner */
    if ( fstat(fileno(f), &st) != 0 
	 || st.st_uid != rootuid || st.st_gid != fcrongid
	 || st.st_mode & S_IWGRP || st.st_mode & S_IWOTH ) {
	error("Conf file (%s) must be owned by root:" GROUPNAME 
	      " and (no more than) 644 : ignored", fcronconf, GROUPNAME);
	fclose(f);
	return;
    }
    
    while ( (ptr1 = fgets(buf, sizeof(buf), f)) != NULL ) {

	Skip_blanks(ptr1); /* at the beginning of the line */
	
	/* ignore comments and blank lines */
	if ( *ptr1 == '#' || *ptr1 == '\n' || *ptr1 == '\0')
	    continue;

	remove_blanks(ptr1); /* at the end of the line */

	/* get the name of the var */
	if ( ( namesize = get_word(&ptr1) ) == 0 )
	    /* name is zero-length */
	    error("Zero-length var name at line %s : line ignored", buf);

	ptr2 = ptr1 + namesize;

	/* skip the blanks and the "=" and go to the value */
	while ( isspace( (int) *ptr2 ) ) ptr2++;
	if ( *ptr2 == '=' ) ptr2++;
	while ( isspace( (int) *ptr2 ) ) ptr2++;

	/* find which var the line refers to and update it */
	if ( strncmp(ptr1, "fcrontabs", namesize) == 0 )
	    fcrontabs = strdup2(ptr2);
	else if ( strncmp(ptr1, "pidfile", namesize) == 0 )
	    pidfile = strdup2(ptr2);
	else if ( strncmp(ptr1, "fifofile", namesize) == 0 )
	    fifofile = strdup2(ptr2);
	else if ( strncmp(ptr1, "fcronallow", namesize) == 0 )
	    fcronallow = strdup2(ptr2);
	else if ( strncmp(ptr1, "fcrondeny", namesize) == 0 )
	    fcrondeny = strdup2(ptr2);
	else if ( strncmp(ptr1, "shell", namesize) == 0 )
	    shell = strdup2(ptr2);
	else if ( strncmp(ptr1, "sendmail", namesize) == 0 )
	    sendmail = strdup2(ptr2);
	else if ( strncmp(ptr1, "editor", namesize) == 0 )
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
/*   	debug("  fifofile=%s", fifofile); */
/*  	debug("  editor=%s", editor); */
/*  	debug("  shell=%s", shell); */
/*  	debug("  sendmail=%s", sendmail); */
    }

    fclose(f);

}
