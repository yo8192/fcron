
/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000 Thibault Godouet <fcron@free.fr>
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

 /* $Id: job.c,v 1.10 2000-06-19 12:43:27 thib Exp $ */

#include "fcron.h"

int temp_file(void);
void xwrite(int fd, char *string);
void launch_mailer(CL *line, int mailfd);
int change_user(const char *user);
void sig_dfl(void);
void end_job(CL *line, int status, int mailfd, short mailpos);
void end_mailer(CL *line, int status);


int
change_user(const char *user)
{
    struct passwd *pas;


    /* Obtain password entry and change privileges */

    if ((pas = getpwnam(user)) == NULL) 
        die("failed to get uid for %s", user);
    
    setenv("USER", pas->pw_name, 1);
    setenv("HOME", pas->pw_dir, 1);
    setenv("SHELL", pas->pw_shell, 1);

    /* Change running state to the user in question */

    if (initgroups(user, pas->pw_gid) < 0)
	die_e("initgroups failed: %s", user);

    if (setregid(pas->pw_gid, pas->pw_gid) < 0) 
	die("setregid failed: %s %d", user, pas->pw_gid);
    
    if (setreuid(pas->pw_uid, pas->pw_uid) < 0) 
	die("setreuid failed: %s %d", user, pas->pw_uid);

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

void 
run_job(CL *line)
    /* fork(), redirect outputs to a temp file, and execl() the task */ 
{

    pid_t pid;
    struct job *j;

    /* append job to the list of executed job */
    Alloc(j, job);
    j->j_line = line;
    j->j_next = exe_base;
    exe_base = j;

    /* prepare the job execution */
    switch ( pid = fork() ) {
    case -1:
	error_e("Fork error : could not exec '%s'", line->cl_shell);
	break;

    case 0:
	/* child */
    {
	char *shell;
	char *home;
	env_t *env;
	int mailfd = 0;
	short int mailpos = 0;	/* 'empty mail file' size */
	int status = 0;

	foreground = 0;
	if (change_user(line->cl_file->cf_user) < 0)
	    return ;

	sig_dfl();

	if ( ! is_mail(line->cl_option) ) {
	    if ( (mailfd = open("/dev/null", O_RDWR)) < 0 )
		die_e("open: /dev/null:");
	}
	else {
	    /* create temporary file for stdout and stderr of the job */
	    mailfd = temp_file();

	    /* write mail header */
	    xwrite(mailfd,"To: ");
	    xwrite(mailfd, line->cl_file->cf_user);
	    xwrite(mailfd, "\nSubject: Output of fcron job: '");
	    xwrite(mailfd, line->cl_shell);
	    xwrite(mailfd,"'\n\n");
	    mailpos = ( lseek(mailfd, 0, SEEK_END) - strlen(line->cl_shell) );
	}

	if (dup2(mailfd, 1) != 1 || dup2(1, 2) != 2)
	    die_e("dup2() error");    /* dup2 also clears close-on-exec flag */

	foreground = 1; 
	/* now, errors will be mailed to the user (or to /dev/null) */

	xcloselog();

	/* set env variables */
	for ( env = line->cl_file->cf_env_base; env; env = env->e_next)
	    if ( setenv(env->e_name, env->e_val, 1) != 0 )
		error("could not setenv()");

	if ( (home = getenv("HOME")) != NULL )
	    if (chdir(home) != 0) {
		error_e("Could not chdir to HOME dir '%s'", home);
		if (chdir("/") < 0)
		    die_e("Could not chdir to HOME dir '/'");
	    }

	if ( (shell = getenv("SHELL")) == NULL )
	    shell = SHELL;
	else if ( access(shell, X_OK) != 0 ) {
	    if (errno == ENOENT)
		error("shell '%s' : no file or directory. SHELL set to " SHELL,
		      shell);
	    else
		error_e("shell '%s' not valid : SHELL set to " SHELL, shell);
	    shell = SHELL;
	}


	/* now, run the job */
	switch ( fork() ) {
	case -1:
	    error_e("Fork error : could not exec '%s'", line->cl_shell);
	    break;

	case 0:
	    /* child */

#ifdef CHECKJOBS
	    /* this will force to mail a message containing at least the exact
	     * and complete command executed for each execution of all jobs */
	    debug("Execing '%s -c %s'", shell, line->cl_shell);
#endif /* CHECKJOBS */

	    execl(shell, shell, "-c", line->cl_shell, NULL);

	    /* execl returns only on error */
	    die_e("execl() '%s -c %s' error", shell, line->cl_shell);

	    /* execution never gets here */

	default:
	    /* parent */
	    /* we use a while because an possible interruption by a signal */
	    while ( (pid = wait3(&status, 0, NULL)) > 0) {
		end_job(line, status, mailfd, mailpos);

		/* execution never gets here */

	    }
	    
	}
    }

    default:
	/* parent */

	line->cl_pid = pid;
	line->cl_file->cf_running += 1;
	explain("  Job `%s' started (pid %d)", line->cl_shell, line->cl_pid);

    }

}

void 
end_job(CL *line, int status, int mailfd, short mailpos)
    /* if task have made some output, mail it to user */
{

    char mail_output;
    char *m;


    if ( is_mail(line->cl_option) &&
	 ( lseek(mailfd, 0, SEEK_END) - strlen (line->cl_shell) ) > mailpos ) {
	    /* an output exit : we will mail it */
	    mail_output = 1;
    }
    else
	/* no output */
	mail_output = 0;

    m= (mail_output == 1) ? " (mailing output)" : "";
    if (WIFEXITED(status) && WEXITSTATUS(status)==0) {
	foreground = 0;
	explain("Job `%s' terminated%s", line->cl_shell, m);
    }
    else if (WIFEXITED(status))
	explain("Job `%s' terminated (exit status: %d)%s",
		line->cl_shell, WEXITSTATUS(status), m);
    else if (WIFSIGNALED(status))
	error("Job `%s' terminated due to signal %d%s",
		 line->cl_shell, WTERMSIG(status), m);
    else /* is this possible? */
	error("Job `%s' terminated abnormally %s", line->cl_shell, m);

    if (mail_output == 1) launch_mailer(line, mailfd);

    /* if MAILTO is "", temp file is already closed */
    if ( close(mailfd) != 0 )
	die_e("Can't close file descriptor %d", mailfd);

    exit(0);

}

void
launch_mailer(CL *line, int mailfd)
    /* mail the output of a job to user */
{
    char *mailto = NULL;

    foreground = 0;

    /* set stdin to the job's output */
    if (dup2(mailfd,0)!=0) die_e("Can't dup2()");
    if (lseek(0,0,SEEK_SET)!=0) die_e("Can't lseek()");

    xcloselog();

    /* determine which will be the mail receiver */
    if ( (mailto = line->cl_file->cf_mailto) == NULL )
	mailto = line->cl_file->cf_user;

    /* run sendmail with mail file as standard input */
    execl(SENDMAIL, SENDMAIL, SENDMAIL_ARGS, mailto, NULL);
    die_e("Can't exec " SENDMAIL);

}

int
temp_file(void)
    /* Open a temporary file and return its file descriptor */
{
    const int max_retries = 50;
    char *name;
    int fd,i;

    i = 0;
    name = NULL;
    do {
	i++;
	free(name);
	name = tempnam(NULL, NULL);
	if (name == NULL) die("Can't find a unique temporary filename");
	fd = open(name, O_RDWR|O_CREAT|O_EXCL|O_APPEND,S_IRUSR|S_IWUSR);
	/* I'm not sure we actually need to be so persistent here */
    } while (fd == -1 && errno == EEXIST && i < max_retries);
    if (fd == -1) die_e("Can't open temporary file");
    if (unlink(name)) die_e("Can't unlink temporary file");
    free(name);
    fcntl(fd, F_SETFD,1);   /* set close-on-exec flag */
    return fd;
}


void
xwrite(int fd, char *string)
  /* Write (using write()) the string "string" to temporary file "fd".
   * Don't return on failure */
{
    if ( write(fd, string, strlen(string)) == -1 )
	die_e("Can't write to temporary file");
}

