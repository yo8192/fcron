
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

 /* $Id: job.c,v 1.48 2001-12-23 22:03:46 thib Exp $ */

#include "fcron.h"

#include "job.h"

void sig_dfl(void);
void end_job(CL *line, int status, int mailfd, short mailpos);
void end_mailer(CL *line, int status);
#ifdef HAVE_LIBPAM
void die_mail_pame(CL *cl, int pamerrno, struct passwd *pas, char *str);
#endif

#ifndef HAVE_SETENV
char env_user[PATH_LEN];
char env_home[PATH_LEN];
char env_shell[PATH_LEN];
#endif

#ifdef HAVE_LIBPAM
void
die_mail_pame(CL *cl, int pamerrno, struct passwd *pas, char *str)
/* log an error in syslog, mail user if necessary, and die */
{
    char buf[MAX_MSG];

    strcpy(buf, str);
    strcat(buf, " for '%s'");

    if (is_mail(cl->cl_option)) {
	int mailfd = create_mail(cl, "Could not run fcron job");

	/* print the error in both syslog and a file, in order to mail it to user */
	if (dup2(mailfd, 1) != 1 || dup2(1, 2) != 2)
	    die_e("dup2() error");    /* dup2 also clears close-on-exec flag */

	foreground = 1;
	error_pame(pamh, pamerrno, buf, cl->cl_shell);
	error("Job '%s' has *not* run.", cl->cl_shell);
	foreground = 0;

	pam_end(pamh, pamerrno);  

	/* Change running state to the user in question : it's safer to run the mail 
	 * as user, not root */
	if (initgroups(pas->pw_name, pas->pw_gid) < 0)
	    die_e("initgroups failed: %s", pas->pw_name);
	if (setgid(pas->pw_gid) < 0) 
	    die("setgid failed: %s %d", pas->pw_name, pas->pw_gid);
	if (setuid(pas->pw_uid) < 0) 
	    die("setuid failed: %s %d", pas->pw_name, pas->pw_uid);

	launch_mailer(cl, mailfd);
	/* launch_mailer() does not return : we never get here */
    }
    else
	die_pame(pamh, pamerrno, buf, cl->cl_shell);
}
#endif

int
change_user(struct CL *cl)
{
    struct passwd *pas;
#ifdef HAVE_LIBPAM
    int    retcode = 0;
    const char * const * env;
#endif

    /* First, restore umask to default */
    umask (saved_umask);

    /* Obtain password entry and change privileges */

    if ((pas = getpwnam(cl->cl_runas)) == NULL) 
        die("failed to get passwd fields for user \"%s\"", cl->cl_runas);
    
#ifdef HAVE_SETENV
    setenv("USER", pas->pw_name, 1);
    setenv("HOME", pas->pw_dir, 1);
    setenv("SHELL", pas->pw_shell, 1);
#else
    {
	strcat( strcpy(env_user, "USER"), "=");
	putenv( strncat(env_user, pas->pw_name, sizeof(env_user)-6) );
	strcat( strcpy(env_home, "HOME"), "=");
	putenv( strncat(env_home, pas->pw_dir, sizeof(env_home)-6) );
	strcat( strcpy(env_shell, "SHELL"), "=");
	putenv( strncat(env_shell, pas->pw_shell, sizeof(env_shell)-7) );
    }
#endif /* HAVE_SETENV */

#ifdef HAVE_LIBPAM
    /* Open PAM session for the user and obtain any security
       credentials we might need */

    retcode = pam_start("fcron", pas->pw_name, &apamconv, &pamh);
    if (retcode != PAM_SUCCESS) die_pame(pamh, retcode, "Could not start PAM for %s",
					 cl->cl_shell);
    retcode = pam_acct_mgmt(pamh, PAM_SILENT); /* permitted access? */
    if (retcode != PAM_SUCCESS) die_mail_pame(cl, retcode, pas,
					      "Could not init PAM account management");
    retcode = pam_setcred(pamh, PAM_ESTABLISH_CRED | PAM_SILENT);
    if (retcode != PAM_SUCCESS) die_mail_pame(cl, retcode, pas, 
					      "Could not set PAM credentials");
    retcode = pam_open_session(pamh, PAM_SILENT);
    if (retcode != PAM_SUCCESS) die_mail_pame(cl, retcode, pas,
					      "Could not open PAM session");

    env = (const char * const *) pam_getenvlist(pamh);
    while (env && *env) {
	if (putenv((char*) *env)) die_e("Could not copy PAM environment");
	env++;
    }

    /* Close the log here, because PAM calls openlog(3) and
       our log messages could go to the wrong facility */
    xcloselog();
#endif /* USE_PAM */

    /* Change running state to the user in question */
    if (initgroups(pas->pw_name, pas->pw_gid) < 0)
	die_e("initgroups failed: %s", pas->pw_name);

    if (setgid(pas->pw_gid) < 0) 
	die("setgid failed: %s %d", pas->pw_name, pas->pw_gid);
    
    if (setuid(pas->pw_uid) < 0) 
	die("setuid failed: %s %d", pas->pw_name, pas->pw_uid);

    return(pas->pw_uid);
}


void
sig_dfl(void)
    /* set signals handling to its default */
{
	signal(SIGTERM, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
}


int
create_mail(CL *line, char *subject) 
    /* create a temp file and write in it a mail header */
{
    /* create temporary file for stdout and stderr of the job */
    int mailfd = temp_file(NULL);
    
    /* write mail header */
    xwrite(mailfd,"To: ");
    xwrite(mailfd, line->cl_file->cf_user);
#ifdef HAVE_GETHOSTNAME
    {
	char hostname[USER_NAME_LEN];
	memset(hostname, 0, sizeof(hostname));
	if (gethostname(hostname, sizeof(hostname)) != 0)
	    error_e("Could not get hostname");
	else {
	    xwrite(mailfd, "@");
	    xwrite(mailfd, hostname);
	}
    }
#endif /* HAVE_GETHOSTNAME */
    xwrite(mailfd, "\nSubject: ");
    xwrite(mailfd, subject);
    xwrite(mailfd, ": '");
    xwrite(mailfd, line->cl_shell);
    xwrite(mailfd,"'\n\n");

    return mailfd;
}

void 
run_job(struct exe *exeent)
    /* fork(), redirect outputs to a temp file, and execl() the task */ 
{

    pid_t pid;
    CL *line = exeent->e_line;

/*      // */
/*      debug("run_job"); */
/*      // */

    /* prepare the job execution */
    switch ( pid = fork() ) {
    case -1:
	error_e("Fork error : could not exec \"%s\"", line->cl_shell);
	break;

    case 0:
	/* child */
    {
	char *curshell;
	char *home;
	env_t *env;
	int mailfd = 0;
	short int mailpos = 0;	/* 'empty mail file' size */
	int status = 0;

	foreground = 0;

	/* we create the temp file (if needed) before change_user(),
	 * as temp_file() need the root privileges */
	if ( ! is_mail(line->cl_option) ) {
	    if ( (mailfd = open("/dev/null", O_RDWR)) < 0 )
		die_e("open: /dev/null:");
	}
	else {
	    mailfd = create_mail(line, "Output of fcron job");
	    mailpos = lseek(mailfd, 0, SEEK_END);
	}

	if (change_user(line) < 0)
	    return ;

	sig_dfl();

	if ( line->cl_nice != 0 && nice(line->cl_nice) != 0 )
	    error_e("could not set nice value");
	
	if (dup2(mailfd, 1) != 1 || dup2(1, 2) != 2)
	    die_e("dup2() error");    /* dup2 also clears close-on-exec flag */

	foreground = 1; 
	/* now, errors will be mailed to the user (or to /dev/null) */

	xcloselog();

	/* set env variables */
	for ( env = line->cl_file->cf_env_base; env; env = env->e_next)
	    if ( putenv(env->e_val) != 0 )
		error("could not putenv()");

	if ( (home = getenv("HOME")) != NULL )
	    if (chdir(home) != 0) {
		error_e("Could not chdir to HOME dir \"%s\"", home);
		if (chdir("/") < 0)
		    die_e("Could not chdir to HOME dir /");
	    }

	if ( (curshell = getenv("SHELL")) == NULL )
	    curshell = shell;
	else if ( access(curshell, X_OK) != 0 ) {
	    if (errno == ENOENT)
		error("shell \"%s\" : no file or directory. SHELL set to %s",
		      curshell, shell);
	    else
		error_e("shell \"%s\" not valid : SHELL set to %s",
			curshell, shell);
	    curshell = shell;
	}


	/* now, run the job */
	switch ( pid = fork() ) {
	case -1:
	    error_e("Fork error : could not exec \"%s\"", line->cl_shell);
	    break;

	case 0:
	    /* child */

#ifdef CHECKJOBS
	    /* this will force to mail a message containing at least the exact
	     * and complete command executed for each execution of all jobs */
	    debug("Execing \"%s -c %s\"", curshell, line->cl_shell);
#endif /* CHECKJOBS */

	    execl(curshell, curshell, "-c", line->cl_shell, NULL);
	    /* execl returns only on error */
	    error_e("Can't find \"%s\". Trying a execlp(\"sh\",...)",curshell);
	    execlp("sh", "sh",  "-c", line->cl_shell, NULL);
	    die_e("execl() \"%s -c %s\" error", curshell, line->cl_shell);

	    /* execution never gets here */

	default:
	    /* parent */
	    if ( ! is_nolog(line->cl_option) ) {
		foreground = 0;
		explain("Job %s started for user %s (pid %d)", line->cl_shell,
			line->cl_file->cf_user, pid);
		foreground = 1;
	    }
	    /* we use a while because of a possible interruption by a signal */
	    while ( (pid = wait3(&status, 0, NULL)) > 0) {
		end_job(line, status, mailfd, mailpos);

		/* execution never gets here */

	    }
	    
	}
    }

    default:
	/* parent */

	exeent->e_pid = pid;
	line->cl_file->cf_running += 1;
    }

}

void 
end_job(CL *line, int status, int mailfd, short mailpos)
    /* if task have made some output, mail it to user */
{

    char mail_output;
    char *m;

    if (is_mailzerolength(line->cl_option) || 
	( ( is_mail(line->cl_option) &&
	    ( lseek(mailfd, 0, SEEK_END) > mailpos ||
	      ! (WIFEXITED(status) && WEXITSTATUS(status) == 0) ) ) ) )
	/* an output exit : we will mail it */
	mail_output = 1;
    else
	/* no output */
	mail_output = 0;

    m = (mail_output == 1) ? " (mailing output)" : "";
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
	foreground = 0;
	debug("Job %s terminated%s", line->cl_shell, m);
    }
    else if (WIFEXITED(status))
	warn("Job %s terminated (exit status: %d)%s",
	     line->cl_shell, WEXITSTATUS(status), m);
    else if (WIFSIGNALED(status))
	error("Job %s terminated due to signal %d%s",
	      line->cl_shell, WTERMSIG(status), m);
    else /* is this possible? */
	error("Job %s terminated abnormally %s", line->cl_shell, m);

#ifdef HAVE_LIBPAM
    /* we close the PAM session before running the mailer command :
     * it avoids a fork(), and we use PAM anyway to control whether a user command
     * should be run or not.
     * We consider that the administrator can use a PAM compliant mailer to control
     * whether a mail can be sent or not.
     * It should be ok like that, otherwise contact me ... -tg */

    /* Aiee! we may need to be root to do this properly under Linux.  Let's
       hope we're more l33t than PAM and try it as non-root. If someone
       complains, I'll fix this :P -hmh */
    pam_setcred(pamh, PAM_DELETE_CRED | PAM_SILENT);
    pam_end(pamh, pam_close_session(pamh, PAM_SILENT));
#endif

    if (mail_output == 1) {
	launch_mailer(line, mailfd);
	/* never reached */
	die_e("Internal error: launch_mailer returned");
    }

    /* if mail is sent, execution doesn't get here : close /dev/null */
    if ( close(mailfd) != 0 )
	die_e("Can't close file descriptor %d", mailfd);

    exit(0);

}

void
launch_mailer(CL *line, int mailfd)
    /* mail the output of a job to user */
{
#ifdef SENDMAIL
    foreground = 0;

    /* set stdin to the job's output */
    if ( dup2(mailfd, 0) != 0 ) die_e("Can't dup2()");
    if ( lseek(0, 0, SEEK_SET ) != 0) die_e("Can't lseek()");

    xcloselog();

    /* run sendmail with mail file as standard input */
    execl(sendmail, sendmail, SENDMAIL_ARGS, line->cl_mailto, NULL);
    error_e("Can't find \"%s\". Trying a execlp(\"sendmail\")", sendmail);
    execlp("sendmail", "sendmail", SENDMAIL_ARGS, line->cl_mailto, NULL);
    die_e("Can't exec " SENDMAIL);
#else /* defined(SENDMAIL) */
    exit(EXIT_OK);
#endif
}


void
xwrite(int fd, char *string)
  /* Write (using write()) the string "string" to temporary file "fd".
   * Don't return on failure */
{
    if ( write(fd, string, strlen(string)) == -1 )
	die_e("Can't write to temporary file");
}

