
/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000 Thibault Godouet <sphawk@free.fr>
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

 /* $Id: job.c,v 1.4 2000-05-22 17:37:39 thib Exp $ */

#include "fcron.h"

int temp_file(void);
void xwrite(int fd, char *string);
void launch_mailer(CF *file, CL *line);
int change_user(const char *user, short dochdir);


int
change_user(const char *user, short dochdir)
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

    if (dochdir) {
	if (chdir(pas->pw_dir) < 0) {
	    error("chdir failed: %s '%s'", user, pas->pw_dir);
	    if (chdir("/") < 0) {
		error("chdir failed: %s '%s'", user, pas->pw_dir);
		die("chdir failed: %s '/'", user);
	    }
	}
    }
    return(pas->pw_uid);
}



void 
run_job(CF *file, CL *line)
    /* fork(), redirect outputs to a temp file, and execl() the task */ 
{

    pid_t pid = 0;
    char *shell = NULL;
    char *home = NULL;
    env_t *env = NULL;

    /* create temporary file for stdout and stderr of the job */
    line->cl_mailfd = temp_file();

    /* write mail header */
    xwrite(line->cl_mailfd,"To: ");
    xwrite(line->cl_mailfd, file->cf_user);
    xwrite(line->cl_mailfd, "\nSubject: Output of fcron job: '");
    xwrite(line->cl_mailfd, line->cl_shell);
    xwrite(line->cl_mailfd,"'\n\n");
    line->cl_mailpos = lseek(line->cl_mailfd, 0, SEEK_END);

    switch ( pid = fork() ) {

    case 0:
	/* child */

	foreground = 0;
	if (change_user(file->cf_user, 1) < 0)
	    return ;

	/* stdin is already /dev/null, setup stdout and stderr */
	if ( close(1) != 0 )
	    die_e("Can't close file descriptor %d",1);
	if ( close(2) != 0 )
	    die_e("Can't close file descriptor %d",2);

	if ( file->cf_mailto != NULL && strcmp(file->cf_mailto, "") == 0 ) {
	    if ( close(line->cl_mailfd) != 0 )
		die_e("Can't close file descriptor %d", line->cl_mailfd);
	    if ( (line->cl_mailfd = open("/dev/null", O_RDWR)) < 0 )
		die_e("open: /dev/null:");
	}

	if (dup2(line->cl_mailfd, 1) != 1 || dup2(line->cl_mailfd, 2) != 2)
	    die_e("dup2() error");    /* dup2 also clears close-on-exec flag */

	foreground = 1; 
	/* now, errors will be mailed to the user (or to /dev/null) */

	xcloselog();

	/* set env variables */
	for ( env = file->cf_env_base; env; env = env->e_next)
	    if ( setenv(env->e_name, env->e_val, 1) != 0 )
		error("could not setenv()");

	if ( (home = getenv("HOME")) != NULL )
	    if (chdir(home) != 0)
		error_e("Could not chdir to HOME dir '%s'", home);

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

#ifdef CHECKJOBS
	/* this will force to mail a message containing at least the exact
	 * and complete command executed for each execution of all jobs */
	debug("Execing '%s -c %s'", shell, line->cl_shell);
#endif /* CHECKJOBS */

	execl(shell, shell, "-c", line->cl_shell, NULL);

	/* execl returns only on error */
	die_e("execl() '%s -c %s' error", shell, line->cl_shell);

	/* execution never gets here */

    case -1:
	error_e("Fork error : could not exec '%s'", line->cl_shell);
	break;

    default:
	/* parent */

	////////
	debug("run job - parent");
	////////

	line->cl_pid = pid;

	////////////
	debug("   cf_running: %d", file->cf_running);
	///////////

	file->cf_running += 1;

	explain("  Job `%s' started (pid %d)", line->cl_shell, line->cl_pid);

    }

}

void 
end_job(CF *file, CL *line, int status)
    /* if task have made some output, mail it to user */
{

    char mail_output;
    char *m;

//////
    debug("   end_job");
//////

    if ( lseek(line->cl_mailfd, 0, SEEK_END) > line->cl_mailpos ) {
	if ( file->cf_mailto != NULL && file->cf_mailto[0] == '\0' )
	    /* there is a mail output, but it will not be mail */
	    mail_output = 2;
	else
	    /* an output exit : we will mail it */
	    mail_output = 1;
    }
    else
	/* no output */
	mail_output = 0;

    m= (mail_output == 1) ? " (mailing output)" : "";
    if (WIFEXITED(status) && WEXITSTATUS(status)==0)
	explain("Job `%s' terminated%s", line->cl_shell, m);
    else if (WIFEXITED(status))
	explain("Job `%s' terminated (exit status: %d)%s",
		line->cl_shell, WEXITSTATUS(status), m);
    else if (WIFSIGNALED(status))
	error("Job `%s' terminated due to signal %d%s",
		 line->cl_shell, WTERMSIG(status), m);
    else /* is this possible? */
	error("Job `%s' terminated abnormally %s", line->cl_shell, m);

    if (mail_output == 1) launch_mailer(file, line);

    /* if MAILTO is "", temp file is already closed */
    if ( mail_output != 2 && close(line->cl_mailfd) != 0 )
	die_e("Can't close file descriptor %d", line->cl_mailfd);

    line->cl_pid = 0;

    ////////////
    debug("    cf_running: %d", file->cf_running);

    file->cf_running -= 1;

}

void
launch_mailer(CF *file, CL *line)
    /* mail the output of a job to user */
{
    char *mailto = NULL;

////////
    debug("   launch mailer");
////////
    switch ( line->cl_mailpid = fork() ) {
    case 0:
	/* child */

	foreground = 0;

	/* set stdin to the job's output */
	if ( close(0) != 0 )
	    die_e("Can't close file descriptor %d",0);
	    

	if (dup2(line->cl_mailfd,0)!=0) die_e("Can't dup2()");
	if (lseek(0,0,SEEK_SET)!=0) die_e("Can't lseek()");

	xcloselog();

	/* determine which will be the mail receiver */
	if ( (mailto = file->cf_mailto) == NULL )
	    mailto = file->cf_user;

	/* change permissions */
	if (change_user(file->cf_user, 1) < 0)
	    return ;

	/* run sendmail with mail file as standard input */
	execl(SENDMAIL, SENDMAIL, SENDMAIL_ARGS, mailto, NULL);
	die_e("Can't exec " SENDMAIL);
	break;

    case -1:
	error_e("Could not exec '%s'", line->cl_shell);
	break;

    default:
	/* parent */

    }
}

void
end_mailer(CL *line, int status)
    /* take care of a finished mailer */
{
////////
    debug("   end mailer");
////////

    if (WIFEXITED(status) && WEXITSTATUS(status)!=0)
	error("Tried to mail output of job `%s', "
		 "but mailer process (" SENDMAIL ") exited with status %d",
		 line->cl_shell, WEXITSTATUS(status) );
    else if (!WIFEXITED(status) && WIFSIGNALED(status))
	error("Tried to mail output of job `%s', "
		 "but mailer process (" SENDMAIL ") got signal %d",
		 line->cl_shell, WTERMSIG(status) );
    else if (!WIFEXITED(status) && !WIFSIGNALED(status))
	error("Tried to mail output of job `%s', "
		 "but mailer process (" SENDMAIL ") terminated abnormally"
		 , line->cl_shell);

    line->cl_mailpid = 0;
}


int
temp_file(void)
    /* Open a temporary file and return its file descriptor */
{
   const int max_retries=50;
   char *name;
   int fd,i;

   i=0;
   name=NULL;
   do {
      i++;
      free(name);
      name=tempnam(NULL,NULL);
      if (name==NULL) die("Can't find a unique temporary filename");
      fd=open(name,O_RDWR|O_CREAT|O_EXCL|O_APPEND,S_IRUSR|S_IWUSR);
      /* I'm not sure we actually need to be so persistent here */
   } while (fd==-1 && errno==EEXIST && i<max_retries);
   if (fd==-1) die_e("Can't open temporary file");
   if (unlink(name)) die_e("Can't unlink temporary file");
   free(name);
   fcntl(fd,F_SETFD,1);   /* set close-on-exec flag */
   return fd;
}


void
xwrite(int fd, char *string)
  /* Write (using write()) the string "string" to temporary file "fd".
   * Don't return on failure */
{
   if (write(fd,string,strlen(string))==-1)
      die_e("Can't write to temporary file");
}

