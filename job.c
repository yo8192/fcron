
/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2004 Thibault Godouet <fcron@free.fr>
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

 /* $Id: job.c,v 1.59 2004-04-29 19:24:59 thib Exp $ */

#include "fcron.h"

#include "job.h"

void sig_dfl(void);
void end_job(cl_t *line, int status, FILE *mailf, short mailpos);
void end_mailer(cl_t *line, int status);
#ifdef HAVE_LIBPAM
void die_mail_pame(cl_t *cl, int pamerrno, struct passwd *pas, char *str);
#endif

#ifndef HAVE_SETENV
char env_user[PATH_LEN];
char env_home[PATH_LEN];
char env_shell[PATH_LEN];
#endif

#ifdef WITH_SELINUX
extern char **environ;
#endif

#ifdef HAVE_LIBPAM
void
die_mail_pame(cl_t *cl, int pamerrno, struct passwd *pas, char *str)
/* log an error in syslog, mail user if necessary, and die */
{
    char buf[MAX_MSG];

    strcpy(buf, str);
    strcat(buf, " for '%s'");

    if (is_mail(cl->cl_option)) {
	FILE *mailf = create_mail(cl, "Could not run fcron job");

	/* print the error in both syslog and a file, in order to mail it to user */
	if (dup2(fileno(mailf), 1) != 1 || dup2(1, 2) != 2)
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

	launch_mailer(cl, mailf);
	/* launch_mailer() does not return : we never get here */
    }
    else
	die_pame(pamh, pamerrno, buf, cl->cl_shell);
}
#endif

int
change_user(struct cl_t *cl)
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
    /* Some system seem to need that pam_authenticate() call.
     * Anyway, we have no way to authentificate the user :
     * we must set auth to pam_permit. */
    retcode = pam_authenticate(pamh, PAM_SILENT);
    if (retcode != PAM_SUCCESS) die_mail_pame(cl, retcode, pas,
					      "Could not authenticate PAM user");
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
	signal(SIGUSR2, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
}


FILE *
create_mail(cl_t *line, char *subject) 
    /* create a temp file and write in it a mail header */
{
    /* create temporary file for stdout and stderr of the job */
    int mailfd = temp_file(NULL);
    FILE *mailf = fdopen(mailfd, "r+");
    char hostname[USER_NAME_LEN];
    /* is this a complete mail address ? (ie. with a "@", not only a username) */
    char complete_adr = 0;
    int i;

    if ( mailf == NULL )
	die_e("Could not fdopen() mailfd");

    /* write mail header */
    fprintf(mailf, "To: %s", line->cl_mailto);

#ifdef HAVE_GETHOSTNAME
    if (gethostname(hostname, sizeof(hostname)) != 0) {
	error_e("Could not get hostname");
	hostname[0] = '\0';
    }
    else {
	/* it is unspecified whether a truncated hostname is NUL-terminated */
	hostname[USER_NAME_LEN-1] = '\0';

	/* check if mailto is a complete mail address */
	for ( i = 0 ; line->cl_mailto[i] != '\0' ; i++ ) {
	    if ( line->cl_mailto[i] == '@' ) {
		complete_adr = 1;
		break;
	    }
	}
	if ( ! complete_adr )
	    fprintf(mailf, "@%s", hostname);
    }
#else
    hostname[0] = '\0';
#endif /* HAVE_GETHOSTNAME */

    if (subject)
	fprintf(mailf, "\nSubject: fcron <%s@%s> %s: %s\n\n", line->cl_file->cf_user,
		( hostname[0] != '\0')? hostname:"?" , subject, line->cl_shell);
    else
	fprintf(mailf, "\nSubject: fcron <%s@%s> %s\n\n", line->cl_file->cf_user,
		( hostname[0] != '\0')? hostname:"?" , line->cl_shell);


    return mailf;
}

void 
run_job(struct exe_t *exeent)
    /* fork(), redirect outputs to a temp file, and execl() the task */ 
{

    pid_t pid;
    cl_t *line = exeent->e_line;
    int pipe_pid_fd[2];

    /* prepare the job execution */
    if ( pipe(pipe_pid_fd) != 0 ) {
	error_e("pipe(pipe_pid_fd) : setting job_pid to -1");
	exeent->e_job_pid = -1;
    }

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
	FILE *mailf = NULL;
	int status = 0;
 	int to_stdout = foreground && is_stdout(line->cl_option);
	int pipe_fd[2];
	short int mailpos = 0;	/* 'empty mail file' size */
#ifdef WITH_SELINUX
	int flask_enabled = is_selinux_enabled();
#endif
 
	/* */
 	debug("sent output to %s, %s, %s\n", to_stdout ? "stdout" : "file",
 	      foreground ? "running in foreground" : "running in background",
 	      is_stdout(line->cl_option) ? "stdout" : "normal" );
	/* */

	if ( ! to_stdout && is_mail(line->cl_option) ) {
	    /* we create the temp file (if needed) before change_user(),
	     * as temp_file() needs the root privileges */
	    /* if we run in foreground, stdout and stderr point to the console.
	     * Otherwise, stdout and stderr point to /dev/null . */
	    mailf = create_mail(line, NULL);
	    mailpos = ftell(mailf);
	    if (pipe(pipe_fd) != 0) 
		die_e("could not pipe()");
	}

#ifndef RUN_NON_PRIVILEGED
	if (change_user(line) < 0)
	    return ;
#endif

	sig_dfl();

	/* now, run the job */
	switch ( pid = fork() ) {
	case -1:
	    error_e("Fork error : could not exec \"%s\"", line->cl_shell);
	    break;

	case 0:
	    /* child */
	    if ( ! to_stdout ) {
		if (is_mail(line->cl_option) ) {
		    /* we can't dup2 directly to mailfd, since a "cmd > /dev/stderr" in
		     * a script would erase all previously collected message */
		    if ( dup2( pipe_fd[1], 1) != 1 || dup2(1, 2) != 2 )
			die_e("dup2() error");  /* dup2 also clears close-on-exec flag */
		    /* we close the pipe_fd[]s : the resources remain, and the pipe will
		     * be effectively close when the job stops */
		    close(pipe_fd[0]);
		    close(pipe_fd[1]);
		    /* Standard buffering results in unwanted behavior (some messages,
		       at least error from fcron process itself, are lost) */
#ifdef HAVE_SETLINEBUF
		    setlinebuf(stdout);
		    setlinebuf(stderr);
#elif
		    setvbuf(stdout, NULL, _IONBF, 0);
		    setvbuf(stderr, NULL, _IONBF, 0);
#endif
		}
		else if ( foreground ) {
		    freopen("/dev/null", "w", stdout);
		    freopen("/dev/null", "w", stderr);
		}
	    }

	    foreground = 1; 
	    /* now, errors will be mailed to the user (or to /dev/null) */

	    if ( line->cl_nice != 0 ) {
		errno = 0; /* so that we work with any libc and kernel */
		if ( nice(line->cl_nice) == -1  &&  errno != 0 )
		    error_e("could not set nice value");
	    }

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

#ifdef CHECKJOBS
	    /* this will force to mail a message containing at least the exact
	     * and complete command executed for each execution of all jobs */
	    debug("Execing \"%s -c %s\"", curshell, line->cl_shell);
#endif /* CHECKJOBS */

#ifdef WITH_SELINUX
	    if(flask_enabled && setexeccon(line->cl_file->cf_user_context) )
		die_e("Can't set execute context \"%s\".",
		      line->cl_file->cf_user_context);
#endif
	    execl(curshell, curshell, "-c", line->cl_shell, NULL);
	    /* execl returns only on error */
	    error_e("Can't find \"%s\". Trying a execlp(\"sh\",...)",curshell);
	    execlp("sh", "sh",  "-c", line->cl_shell, NULL);
	    die_e("execl() \"%s -c %s\" error", curshell, line->cl_shell);

	    /* execution never gets here */

	default:
	    /* parent */

	    /* close unneeded WRITE pipe and READ pipe */
	    close(pipe_fd[1]);
	    close(pipe_pid_fd[0]);

	    /* give the pid of the child to the parent (main) fcron process */
	    if ( write(pipe_pid_fd[1], &pid, sizeof(pid)) < 0 )
		error_e("could not write child pid to pipe_pid_fd[1]");
	    close(pipe_pid_fd[1]);

	    if ( ! is_nolog(line->cl_option) )
		explain("Job %s started for user %s (pid %d)", line->cl_shell,
			line->cl_file->cf_user, pid);

	    if ( ! to_stdout && is_mail(line->cl_option ) ) {
		/* user wants a mail : we use the pipe */
		char mailbuf[TERM_LEN];
		FILE *pipef = fdopen(pipe_fd[0], "r");

		if ( pipef == NULL )
		    die_e("Could not fdopen() pipe_fd[0]");

		mailbuf[sizeof(mailbuf)-1] = '\0';
		while ( fgets(mailbuf, sizeof(mailbuf), pipef) != NULL )
		    if ( fputs(mailbuf, mailf) < 0 )
			warn("fputs() failed to write to mail file for job %s (pid %d)",
			     line->cl_shell, pid);
		fclose(pipef); /* (closes also pipe_fd[0]) */
	    }

	    /* we use a while because of a possible interruption by a signal */
	    while ( (pid = wait3(&status, 0, NULL)) > 0)
		end_job(line, status, mailf, mailpos);

		/* execution never gets here */
	}
    }

    default:
	/* parent */

	/* close unneeded WRITE fd */
	close(pipe_pid_fd[1]);

	exeent->e_ctrl_pid = pid;
	line->cl_file->cf_running += 1;

	/* read the pid of the job */
	if (read(pipe_pid_fd[0], &(exeent->e_job_pid), sizeof(pid_t)) < sizeof(pid_t)) {
	    error_e("Could not read job pid : setting it to -1");
	    exeent->e_job_pid = -1;
	}
	close(pipe_pid_fd[0]);
    }

}

void 
end_job(cl_t *line, int status, FILE *mailf, short mailpos)
    /* if task have made some output, mail it to user */
{

    char mail_output;
    char *m;

    if ( mailf != NULL &&
	 (is_mailzerolength(line->cl_option) || 
	  ( ( is_mail(line->cl_option) &&
	      ( (fseek(mailf, 0, SEEK_END) == 0 && ftell(mailf) > mailpos) ||
		! (WIFEXITED(status) && WEXITSTATUS(status) == 0) ) ) ) ) )
	/* an output exit : we will mail it */
	mail_output = 1;
    else
	/* no output */
	mail_output = 0;

    m = (mail_output == 1) ? " (mailing output)" : "";
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
	if ( ! is_nolog(line->cl_option) )
	    explain("Job %s completed%s", line->cl_shell, m);
    }
    else if (WIFEXITED(status)) {
	warn("Job %s terminated (exit status: %d)%s",
	     line->cl_shell, WEXITSTATUS(status), m);
	/* there was an error : in order to inform the user by mail, we need
	 * to add some data to mailf */
	if ( mailf != NULL )
	    fprintf(mailf, "Job %s terminated (exit status: %d)%s",
		    line->cl_shell, WEXITSTATUS(status), m);
    }
    else if (WIFSIGNALED(status)) {
	error("Job %s terminated due to signal %d%s",
	      line->cl_shell, WTERMSIG(status), m);
	if ( mailf != NULL )
	    fprintf(mailf, "Job %s terminated due to signal %d%s",
		    line->cl_shell, WTERMSIG(status), m);
    }
    else { /* is this possible? */
	error("Job %s terminated abnormally %s", line->cl_shell, m);
	if ( mailf != NULL )
	    fprintf(mailf, "Job %s terminated abnormally %s", line->cl_shell, m);
    }

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
	launch_mailer(line, mailf);
	/* never reached */
	die_e("Internal error: launch_mailer returned");
    }

    /* if mail is sent, execution doesn't get here : close /dev/null */
    if ( mailf != NULL && fclose(mailf) != 0 )
	die_e("Can't close file mailf");

    exit(0);

}

void
launch_mailer(cl_t *line, FILE *mailf)
    /* mail the output of a job to user */
{
#ifdef SENDMAIL
    foreground = 0;

    /* set stdin to the job's output */
    if ( fseek(mailf, 0, SEEK_SET ) != 0) die_e("Can't fseek()");
    if ( dup2(fileno(mailf), 0) != 0 ) die_e("Can't dup2(fileno(mailf))");

    xcloselog();

    if ( chdir("/") < 0 )
	die_e("Could not chdir to /");

    /* run sendmail with mail file as standard input */
    execl(sendmail, sendmail, SENDMAIL_ARGS, line->cl_mailto, NULL);
    error_e("Can't find \"%s\". Trying a execlp(\"sendmail\")", sendmail);
    execlp("sendmail", "sendmail", SENDMAIL_ARGS, line->cl_mailto, NULL);
    die_e("Can't exec " SENDMAIL);
#else /* defined(SENDMAIL) */
    exit(EXIT_OK);
#endif
}


