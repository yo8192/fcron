
/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2002 Thibault Godouet <fcron@free.fr>
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

 /* $Id: fcrontab.c,v 1.58 2002-08-22 21:32:12 thib Exp $ */

/* 
 * The goal of this program is simple : giving a user interface to fcron
 * daemon, by allowing each user to see, modify, append or remove his 
 * fcrontabs. 
 * Fcron daemon use a specific formated format of file, so fcrontab generate
 * that kind of file from human readable files. In order allowing users to
 * see and modify their fcrontabs, the source file is always saved with the
 * formated one. 
 * Fcrontab makes a temporary formated file, and then sends a signal 
 * to the daemon to force it to update its configuration, remove the temp
 * file and save a new and final formated file.
 * That way, not the simple, allows the daemon to keep a maximum of 
 * informations like the time remaining before next execution, or the date
 * and hour of next execution.
 */

#include "fcrontab.h"

#include "allow.h"
#include "fileconf.h"
#include "temp_file.h"
#include "read_string.h"

char rcs_info[] = "$Id: fcrontab.c,v 1.58 2002-08-22 21:32:12 thib Exp $";

void info(void);
void usage(void);


/* used in temp_file() */
char *tmp_path = "/tmp/";

/* command line options */
char rm_opt = 0;
char list_opt = 0;
char edit_opt = 0;
char reinstall_opt = 0;
char ignore_prev = 0;
int file_opt = 0;

#ifdef DEBUG
char debug_opt = 1;       /* set to 1 if we are in debug mode */
#else
char debug_opt = 0;       /* set to 1 if we are in debug mode */
#endif

char *user = NULL;
uid_t uid = 0;
uid_t asuid = 0;
gid_t asgid = 0;
uid_t fcrontab_uid = 0;
gid_t fcrontab_gid = 0;

char need_sig = 0;           /* do we need to signal fcron daemon */

char orig_dir[PATH_LEN];
CF *file_base = NULL;
char buf[PATH_LEN];
char file[PATH_LEN];

/* needed by log part : */
char *prog_name = NULL;
char foreground = 1;
char dosyslog = 1;
pid_t daemon_pid = 0;

#ifdef HAVE_LIBPAM
int conv_pam(int num_msg, const struct pam_message **msgm,
	     struct pam_response **response, void *appdata_ptr);
pam_handle_t *pamh = NULL;
const struct pam_conv apamconv = { conv_pam, NULL };
#endif /* HAVE_LIBPAM */

void
info(void)
    /* print some informations about this program :
     * version, license */
{
    fprintf(stderr,
	    "fcrontab " VERSION_QUOTED " - user interface to daemon fcron\n"
	    "Copyright 2000-2002 Thibault Godouet <fcron@free.fr>\n"
	    "This program is free software distributed WITHOUT ANY WARRANTY.\n"
            "See the GNU General Public License for more details.\n"
	);

    exit(EXIT_OK);

}


void
usage(void)
  /*  print a help message about command line options and exit */
{
    fprintf(stderr, 
	    "fcrontab [-n] file [user|-u user]\n"
	    "fcrontab { -l | -r | -e | -z } [-n] [user|-u user]\n"
	    "fcrontab -h\n"
	    "  -u user    specify user name.\n"
	    "  -l         list user's current fcrontab.\n"
	    "  -r         remove user's current fcrontab.\n"
	    "  -e         edit user's current fcrontab.\n"
	    "  -z         reinstall user's fcrontab from source code.\n"
	    "  -n         ignore previous version of file.\n"
	    "  -c f       make fcrontab use config file f.\n"
	    "  -d         set up debug mode.\n"
	    "  -h         display this help message.\n"
	    "  -V         display version & infos about fcrontab.\n"
	    "\n"
	);
    
    exit(EXIT_ERR);
}


void
xexit(int exit_val)
    /* launch signal if needed and exit */
{
    pid_t pid = 0;

    if ( need_sig == 1 ) {
	
	/* fork and exec fcronsighup */
	switch ( pid = fork() ) {
	case 0:
	    /* child */
	    execl(BINDIREX "/fcronsighup", BINDIREX "/fcronsighup",
		  fcronconf, NULL);
	    die_e("Could not exec " BINDIREX " fcronsighup");
	    break;

	case -1:
	    die_e("Could not fork (fcron has not been signaled)");
	    break;

	default:
	    /* parent */
	    waitpid(pid, NULL, 0);
	    break;
	}
    }

#ifdef HAVE_LIBPAM
    pam_setcred(pamh, PAM_DELETE_CRED | PAM_SILENT);
    pam_end(pamh, pam_close_session(pamh, PAM_SILENT));
#endif

    exit(exit_val);

}


int
copy(char *orig, char *dest)
    /* copy orig file to dest */
{
    FILE *from = NULL, *to = NULL;
    int c;

    if ( (from = fopen(orig, "r")) == NULL) {
	error_e("copy: orig");
	return ERR;
    }
    /* create it as fcrontab_uid (to avoid problem if user's uid changed)
     * except for root. Root requires filesystem uid root for security
     * reasons */
#ifdef USE_SETE_ID
    if (asuid == ROOTUID) {
	if (seteuid(ROOTUID) != 0)
	    error_e("seteuid(ROOTUID)");
    } 
    else {
    	if (seteuid(fcrontab_uid) != 0)
	    error_e("seteuid(fcrontab_uid[%d])", fcrontab_uid);
    }
#endif
    if ((to = fopen(dest, "w")) == NULL) {
	error_e("copy: dest");
	return ERR;
    }
#ifdef USE_SETE_ID
    if (asuid != ROOTUID && seteuid(uid) != 0)
	die_e("seteuid(uid[%d])", uid);
#endif
    if (asuid == ROOTUID ) {
	if ( fchmod(fileno(to), S_IWUSR | S_IRUSR) != 0 )
	    error_e("Could not fchmod %s to 600", dest);
	if ( fchown(fileno(to), ROOTUID, fcrontab_gid) != 0 )
	    error_e("Could not fchown %s to root", dest);
    }

    while ( (c = getc(from)) != EOF )
	if ( putc(c, to) == EOF ) {
	    error("Error while copying file. Aborting.\n");
	    return ERR;
	}

    fclose(from);
    fclose(to);
    
    return OK;
}


int
remove_fcrontab(char rm_orig)
    /* remove user's fcrontab and tell daemon to update his conf */
    /* note : the binary fcrontab is removed by fcron */
{
    int return_val = OK;
    FILE *f;

    if ( rm_orig )
	explain("removing %s's fcrontab", user);

    /* remove source and formated file */
    if ( (rm_orig && remove(buf)) != 0 ) {
	if ( errno == ENOENT )
	    return_val = ENOENT;
	else
	    error_e("could not remove %s", buf);		
    }

#ifdef USE_SETE_ID
    if (seteuid(fcrontab_uid) != 0)
	error_e("seteuid(fcrontab_uid[%d])", fcrontab_uid);
#endif
	    
    /* try to remove the temp file in case he has not
     * been read by fcron daemon */
    snprintf(buf, sizeof(buf), "new.%s", user);
    remove(buf);

    /* finally create a file in order to tell the daemon
     * a file was removed, and launch a signal to daemon */
    snprintf(buf, sizeof(buf), "rm.%s", user);
    f = fopen(buf, "w");
    fclose(f);
    
    need_sig = 1;

#ifdef USE_SETE_ID
    if (seteuid(uid) != 0)
	die_e("seteuid(uid[%d])", uid);
#endif

    return return_val;

}


int
write_file(char *file)
{
    int return_val = OK;

    if (ignore_prev == 1)
	/* if user wants to ignore previous version, we remove it *
	 * ( fcron daemon remove files no longer wanted before
	 *   adding new ones ) */
	remove_fcrontab(0);

    if ( file_base->cf_line_base == NULL ) {
	/* no entries */
	explain("%s's fcrontab contains no entries : removed.", user);
	remove_fcrontab(0);
    } 
    else {
	/* write the binary fcrontab on disk */
	snprintf(buf, sizeof(buf), "new.%s", user);
	if ( save_file(buf) != OK )
	    return_val = ERR;
    }

    /* copy original file to fcrontabs dir */
    snprintf(buf, sizeof(buf), "%s.orig", user);
    if ( copy(file, buf) == ERR )
	return_val = ERR;

    return return_val;
}

int
make_file(char *file)
{

    explain("installing file %s for user %s", file, user);

    /* read file and create a list in memory */
    switch ( read_file(file, user) ) {
    case 2:
    case OK:

	if (write_file(file) == ERR)
	    return ERR;

	/* free memory used to store the list */
	delete_file(user);

	/* tell daemon to update the conf */
	need_sig = 1;
	break;

    case ERR:
	return ERR;
    }

    return OK;
    
}


void
list_file(char *file)
{
    FILE *f = NULL;
    int c;

    explain("listing %s's fcrontab", user);

    if ( (f = fopen(file, "r")) == NULL ) {
	if ( errno == ENOENT ) {
	    explain("user %s has no fcrontab.", user);
	    return ;
	}
	else
	    die_e("User %s could not read file \"%s\"", user, file);
    }
    else {

	while ( (c = getc(f)) != EOF )
	    putchar(c);

	fclose(f);

    }

}

void
edit_file(char *buf)
    /* copy file to a temp file, edit that file, and install it
       if necessary */
{
    char *cureditor = NULL;
    char editorcmd[PATH_LEN];
    pid_t pid;
    int status;
    struct stat st;
    time_t mtime = 0;
    char *tmp_str;
    FILE *f, *fi;
    int file = 0;
    int c;
    char correction = 0;
    short return_val = EXIT_OK;

    explain("fcrontab : editing %s's fcrontab", user);	

    if ((cureditor=getenv("VISUAL")) == NULL || strcmp(cureditor, "\0") == 0 )
	if((cureditor=getenv("EDITOR"))==NULL || strcmp(cureditor, "\0") == 0 )
	    cureditor = editor;
	
    file = temp_file(&tmp_str);
    if ( (fi = fdopen(file, "w")) == NULL ) {
	error_e("could not fdopen");
	goto exiterr;
    }
#ifndef USE_SETE_ID
    if (fchown(file, asuid, asgid) != 0) {
	error_e("Could not fchown %s to asuid and asgid", tmp_str);
	goto exiterr;
    }
#endif
    /* copy user's fcrontab (if any) to a temp file */
    if ( (f = fopen(buf, "r")) == NULL ) {
	if ( errno != ENOENT ) {
	    error_e("could not open file %s", buf);
	    goto exiterr;
	}
	else
	    fprintf(stderr, "no fcrontab for %s - using an empty one\n",
		    user);
    }
    else { 
	/* copy original file to temp file */
	while ( (c=getc(f)) != EOF )
	    putc(c, fi);
	fclose(f);
    }

    fclose(fi);
    close(file);

    do {

	if ( stat(tmp_str, &st) == 0 )
	    mtime = st.st_mtime;
	else {
	    error_e("could not stat \"%s\"", buf);
	    goto exiterr;
	}

	switch ( pid = fork() ) {
	case 0:
	    /* child */
	    if ( uid != ROOTUID ) {
		if (setgid(asgid) < 0) {
		    error_e("setgid(asgid)");
		    goto exiterr;
		}
		if (setuid(asuid) < 0) {
		    error_e("setuid(asuid)");
		    goto exiterr;
		}
	    }

	    snprintf(editorcmd, sizeof(editorcmd), "%s %s", cureditor, tmp_str);
	    execlp(shell, shell, "-c", editorcmd, tmp_str, NULL);
	    error_e("Error while running \"%s\"", cureditor);
	    goto exiterr;

	case -1:
	    error_e("fork");
	    goto exiterr;

	default:
	    /* parent */
	    break ;
	}
	    
	/* only reached by parent */
	waitpid(pid, &status, 0);
	if ( ! WIFEXITED(status) ) {
	    fprintf(stderr, "Editor exited abnormally:"
		    " fcrontab is unchanged.\n");
	    goto exiterr;
	}

#ifndef USE_SETE_ID
	/* we have chown the tmp file to user's name : user may have
	 * linked the tmp file to a file owned by root. In that case, as
	 * fcrontab is setuid root, user may read some informations he is not
	 * allowed to read :
	 * we *must* check that the tmp file is not a link. */

	/* open the tmp file, chown it to root and chmod it to avoid race
	 * conditions */
	/* make sure that the tmp file is not a link */
	{
	    int fd = 0;
	    if ( (fd = open(tmp_str, O_RDONLY)) <= 0 ||
		 fstat(fd, &st) != 0 || ! S_ISREG(st.st_mode) ||
		 S_ISLNK(st.st_mode) || st.st_uid != asuid || st.st_nlink > 1){
		fprintf(stderr, "%s is not a valid regular file.\n", tmp_str);
		close(fd);
		goto exiterr;
	    }
	    if ( fchown(fd, ROOTUID, ROOTGID) != 0 || fchmod(fd, S_IRUSR|S_IWUSR) != 0 ) {
		fprintf(stderr, "Can't chown or chmod %s.\n", tmp_str);
		close(fd);
		goto exiterr;
	    }
	    close(fd);
	}
#endif
	
	/* check if file has been modified */
	if ( stat(tmp_str, &st) != 0 ) {
	    error_e("could not stat %s", tmp_str);
	    goto exiterr;
	}    

	else if ( st.st_mtime > mtime || correction == 1) {

	    correction = 0;

	    switch ( read_file(tmp_str, user) ) {
	    case ERR:
		goto exiterr;
	    case 2:
		fprintf(stderr, "\nFile contains some errors. "
			"Ignore [i] or Correct [c] ? ");
		/* the 2nd getchar() is for the newline char (\n) */
		while ( (c = getchar())	&& c != 'i' && c != 'c' ) {
		    fprintf(stderr, "Please press c to correct, "
			    "or i to ignore: ");
		    while (c != '\n')
			c = getchar();
		}
		if ( c == 'c' ) {
		    /* free memory used to store the list */
		    delete_file(user);
		    correction = 1;
		}
		break;
	    default:
		break;
	    }

	}
	else {
	    fprintf(stderr, "Fcrontab is unchanged :"
		    " no need to install it.\n"); 
	    goto end;
	}

    } while ( correction == 1);

    if ( write_file(tmp_str) != OK )
	return_val = EXIT_ERR;
    
    /* free memory used to store the list */
    delete_file(user);
    
    /* tell daemon to update the conf */
    need_sig = 1;
    
  end:
    if ( remove(tmp_str) != 0 )
	error_e("could not remove %s", tmp_str);
    free(tmp_str);
    xexit (return_val);

  exiterr:
    if ( remove(tmp_str) != 0 )
	error_e("could not remove %s", tmp_str);
    free(tmp_str);
    xexit (EXIT_ERR);

}


int
install_stdin(void)
    /* install what we get through stdin */
{
    int tmp_fd = 0;
    FILE *tmp_file = NULL;
    char *tmp_str = NULL;
    register int c;
    short return_val = EXIT_OK;
	    	    
    tmp_fd = temp_file(&tmp_str);
    
    if( (tmp_file = fdopen(tmp_fd, "w")) == NULL )
	die_e("Could not fdopen file %s", tmp_str);

    while ( (c = getc(stdin)) != EOF )
	putc(c, tmp_file);

    fclose(tmp_file);
    close(tmp_fd);

    if ( make_file(tmp_str) == ERR )
	goto exiterr;
    else
	goto exit;

  exiterr:
	return_val = EXIT_ERR;    
  exit:
    if ( remove(tmp_str) != 0 )
	error_e("Could not remove %s", tmp_str);
    free(tmp_str);
    return return_val;

}

void
reinstall(char *buf)
{
    int i = 0;

    explain("reinstalling %s's fcrontab", user);

    if ( (i = open(buf, O_RDONLY)) < 0) {
	if ( errno == ENOENT ) {
	    fprintf(stderr, "Could not reinstall: user %s has no fcrontab\n",
		    user);
	}
	else
	    fprintf(stderr, "Could not open \"%s\": %s\n", buf,
		    strerror(errno));

	xexit(EXIT_ERR);
    }

    close(0); dup2(i, 0);
    close(i);

    xexit(install_stdin());

}


#ifdef HAVE_LIBPAM
int
conv_pam(int num_msg, const struct pam_message **msgm, struct pam_response **response,
	 void *appdata_ptr)
    /* text based conversation for pam. */
{
    int count = 0;
    struct pam_response *reply;

    if (num_msg <= 0 )
	return PAM_CONV_ERR;

    reply = (struct pam_response *) calloc(num_msg, sizeof(struct pam_response));
    if (reply == NULL) {
	debug("no memory for responses");
	return PAM_CONV_ERR;
    }

    for (count = 0; count < num_msg; ++count) {
	char *string = NULL;

	switch ( msgm[count]->msg_style ) {
	case PAM_PROMPT_ECHO_OFF:
	    string = read_string(CONV_ECHO_OFF,msgm[count]->msg);
	    if (string == NULL) {
		goto failed_conversation;
	    }
	    break;
	case PAM_PROMPT_ECHO_ON:
	    string = read_string(CONV_ECHO_ON,msgm[count]->msg);
	    if (string == NULL) {
		goto failed_conversation;
	    }
	    break;
	case PAM_ERROR_MSG:
	    if (fprintf(stderr,"%s\n",msgm[count]->msg) < 0) {
		goto failed_conversation;
	    }
	    break;
	case PAM_TEXT_INFO:
	    if (fprintf(stdout,"%s\n",msgm[count]->msg) < 0) {
		goto failed_conversation;
	    }
	    break;
	default:
	    fprintf(stderr, "erroneous conversation (%d)\n"
		    ,msgm[count]->msg_style);
	    goto failed_conversation;
	}

	if (string) {                         /* must add to reply array */
	    /* add string to list of responses */

	    reply[count].resp_retcode = 0;
	    reply[count].resp = string;
	    string = NULL;
	}
    }

    /* New (0.59+) behavior is to always have a reply - this is
       compatable with the X/Open (March 1997) spec. */
    *response = reply;
    reply = NULL;

    return PAM_SUCCESS;

failed_conversation:

    if (reply) {
	for (count=0; count<num_msg; ++count) {
	    if (reply[count].resp == NULL) {
		continue;
	    }
	    switch (msgm[count]->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
	    case PAM_PROMPT_ECHO_OFF:
		Overwrite(reply[count].resp);
		free(reply[count].resp);
		break;
	    case PAM_ERROR_MSG:
	    case PAM_TEXT_INFO:
		/* should not actually be able to get here... */
		free(reply[count].resp);
	    }                                            
	    reply[count].resp = NULL;
	}
	/* forget reply too */
	free(reply);
	reply = NULL;
    }

    return PAM_CONV_ERR;
}
#endif /* HAVE_LIBPAM */


void
parseopt(int argc, char *argv[])
  /* set options */
{

    int c;
    extern char *optarg;
    extern int optind, opterr, optopt;
    struct passwd *pass;

    /* constants and variables defined by command line */

    while(1) {
	c = getopt(argc, argv, "u:lrezdnhVc:");
	if (c == EOF) break;
	switch (c) {

	case 'V':
	    info(); break;

	case 'h':
	    usage(); break;

	case 'u':
	    if (getuid() != ROOTUID) {
		fprintf(stderr, "must be privileged to use -u\n");
		xexit(EXIT_ERR);
	    }
	    user = strdup2(optarg) ; 
	    break;

	case 'd':
	    debug_opt = 1; break;

	case 'l':
	    if (rm_opt || edit_opt || reinstall_opt) {
		fprintf(stderr, "Only one of the options -l, -r, -e and -z"
			"may be used simultaneously.\n");
		xexit(EXIT_ERR);
	    }
	    list_opt = 1;
	    rm_opt = edit_opt = reinstall_opt = 0;
	    break;

	case 'r':
	    if (list_opt || edit_opt || reinstall_opt) {
		fprintf(stderr, "Only one of the options -l, -r, -e and -z"
			"may be used simultaneously.\n");
		xexit(EXIT_ERR);
	    }
	    rm_opt = 1;
	    list_opt = edit_opt = reinstall_opt = 0;
	    break;

	case 'e':
	    if (list_opt || rm_opt || reinstall_opt) {
		fprintf(stderr, "Only one of the options -l, -r, -e and -z"
			"may be used simultaneously.\n");
		xexit(EXIT_ERR);
	    }
	    edit_opt = 1;
	    list_opt = rm_opt = reinstall_opt = 0;
	    break;

	case 'z':
	    if (list_opt || rm_opt || edit_opt) {
		fprintf(stderr, "Only one of the options -l, -r, -e and -z"
			"may be used simultaneously.\n");
		xexit(EXIT_ERR);
	    }
	    reinstall_opt = ignore_prev = 1;
	    list_opt = rm_opt = edit_opt = 0;
	    break;

	case 'n':
	    ignore_prev = 1;
	    break;
	    
	case 'c':
	    if ( optarg[0] == '/' ) {
		Set(fcronconf, optarg);
	    }
	    else {
		char buf[PATH_LEN];
		snprintf(buf, sizeof(buf), "%s/%s", orig_dir, optarg);
		Set(fcronconf, buf);
	    }
	    break;

	case ':':
	    fprintf(stderr, "(setopt) Missing parameter.\n");
	    usage();

	case '?':
	    usage();

	default:
	    fprintf(stderr, "(setopt) Warning: getopt returned %c.\n", c);
	}
    }

    /* read fcron.conf and update global parameters */
    read_conf();
    
    /* read the file name and/or user and check validity of the arguments */
    if (argc - optind > 2)
	usage();
    else if (argc - optind == 2 ) {
	if ( list_opt + rm_opt + edit_opt + reinstall_opt == 0 )
	    file_opt = optind++;
	else
	    usage();

	if (getuid() != ROOTUID) {
	    fprintf(stderr, "must be privileged to use -u\n");
	    xexit(EXIT_ERR);
	}
	Set(user, argv[optind]); 
    }
    else if (argc - optind == 1) {
	if ( list_opt + rm_opt + edit_opt + reinstall_opt == 0 )
	    file_opt = optind;
	else {
	    if (getuid() != ROOTUID) {
		fprintf(stderr, "must be privileged to use [user|-u user]\n");
		xexit(EXIT_ERR);
	    }
	    Set(user, argv[optind]); 	    
	}
    }
    else if (list_opt + rm_opt + edit_opt + reinstall_opt != 1)
	usage();

    if ( user == NULL ) {
	/* get user's name using getpwuid() */
	if ( ! (pass = getpwuid(uid)) )
	    die("user \"%s\" is not in passwd file. Aborting.", USERNAME);
	/* we need to strdup2 the value given by getpwuid() because we free
	 * file->cf_user in delete_file */
	user = strdup2(pass->pw_name);
	asuid = pass->pw_uid;
	asgid = pass->pw_gid;
    }
    else {
	if ( ! (pass = getpwnam(user)) )
	    die("user \"%s\" is not in passwd file. Aborting.", user);
	asuid = pass->pw_uid;
	asgid = pass->pw_gid;
    }

    if ( ! is_allowed(user) ) {
	die("User \"%s\" is not allowed to use %s. Aborting.",
	    user, prog_name);	    
    }
}


int
main(int argc, char **argv)
{

#ifdef HAVE_LIBPAM
    int    retcode = 0;
    const char * const * env;
#endif
#ifdef USE_SETE_ID
    struct passwd *pass;
#endif

    memset(buf, 0, sizeof(buf));
    memset(file, 0, sizeof(file));

    if (strrchr(argv[0],'/')==NULL) prog_name = argv[0];
    else prog_name = strrchr(argv[0],'/')+1;
    
    uid = getuid();

    /* get current dir */
    if ( getcwd(orig_dir, sizeof(orig_dir)) == NULL )
	die_e("getcwd");

    /* interpret command line options */
    parseopt(argc, argv);

#ifdef USE_SETE_ID
    if ( ! (pass = getpwnam(USERNAME)) )
	die("user \"%s\" is not in passwd file. Aborting.", USERNAME);
    fcrontab_uid = pass->pw_uid;
    fcrontab_gid = pass->pw_gid;

#ifdef HAVE_LIBPAM
    /* Open PAM session for the user and obtain any security
       credentials we might need */

    debug("username: %s", user);
    retcode = pam_start("fcrontab", user, &apamconv, &pamh);
    if (retcode != PAM_SUCCESS) die_pame(pamh, retcode, "Could not start PAM");
    retcode = pam_authenticate(pamh, 0);    /* is user really user? */
    if (retcode != PAM_SUCCESS)
	die_pame(pamh, retcode, "Could not authenticate user using PAM (%d)", retcode);
    retcode = pam_acct_mgmt(pamh, 0); /* permitted access? */
    if (retcode != PAM_SUCCESS)
	die_pame(pamh, retcode, "Could not init PAM account management (%d)", retcode);
    retcode = pam_setcred(pamh, PAM_ESTABLISH_CRED);
    if (retcode != PAM_SUCCESS) die_pame(pamh, retcode, "Could not set PAM credentials");
    retcode = pam_open_session(pamh, 0);
    if (retcode != PAM_SUCCESS) die_pame(pamh, retcode, "Could not open PAM session");

    env = (const char * const *) pam_getenvlist(pamh);
    while (env && *env) {
	if (putenv((char*) *env)) die_e("Could not copy PAM environment");
	env++;
    }

    /* Close the log here, because PAM calls openlog(3) and
       our log messages could go to the wrong facility */
    xcloselog();
#endif /* USE_PAM */

    if (uid != fcrontab_uid)
	if (seteuid(fcrontab_uid) != 0) 
	    die_e("Couldn't change euid to fcrontab_uid[%d]",fcrontab_uid);
    /* change directory */
    if (chdir(fcrontabs) != 0) {
	error_e("Could not chdir to %s", fcrontabs);
	xexit (EXIT_ERR);
    }
    /* get user's permissions */
    if (seteuid(uid) != 0) 
	die_e("Could not change euid to %d", uid); 
    if (setegid(fcrontab_gid) != 0) 
	die_e("Could not change egid to " GROUPNAME "[%d]", fcrontab_gid); 

#else /* USE_SETE_ID */

    if (setuid(ROOTUID) != 0 ) 
	die_e("Could not change uid to ROOTUID"); 
    if (setgid(ROOTGID) != 0)
    	die_e("Could not change gid to ROOTGID");
    /* change directory */
    if (chdir(fcrontabs) != 0) {
	error_e("Could not chdir to %s", fcrontabs);
	xexit (EXIT_ERR);
    }
#endif /* USE_SETE_ID */
    
    /* this program is seteuid : we set default permission mode
     * to  640 for security reasons */
    umask(026);

    snprintf(buf, sizeof(buf), "%s.orig", user);

    /* determine what action should be taken */
    if ( file_opt ) {

	if ( strcmp(argv[file_opt], "-") == 0 )

	    xexit(install_stdin());

	else {

	    if ( *argv[file_opt] != '/' )
		/* this is just the file name, not the path : complete it */
		snprintf(file, sizeof(file), "%s/%s", orig_dir, argv[file_opt]);
	    else {
		strncpy(file, argv[file_opt], sizeof(file) - 1);
		file[sizeof(file)-1] = '\0';
	    }

	    if (make_file(file) == OK)
		xexit(EXIT_OK);
	    else
		xexit(EXIT_ERR);

	}

    } 

    /* remove user's entries */
    if ( rm_opt == 1 ) {

	if ( remove_fcrontab(1) == ENOENT )
	    fprintf(stderr, "no fcrontab for %s\n", user);

	xexit (EXIT_OK);
    }


    /* list user's entries */
    if ( list_opt == 1 ) {

	list_file(buf);

	xexit(EXIT_OK);

    }


    /* edit user's entries */
    if ( edit_opt == 1 ) {

	edit_file(buf);

	xexit(EXIT_OK);

    }

    /* reinstall user's entries */
    if ( reinstall_opt == 1 ) {

	reinstall(buf);

	xexit(EXIT_OK);

    }

    /* never reached */
    return EXIT_OK;
}
