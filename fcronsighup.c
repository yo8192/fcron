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

 /* $Id: fcronsighup.c,v 1.1 2001-07-07 17:29:51 thib Exp $ */

#include "global.h"

/* log.c */
#include "log.h"
#include "subs.h"
#include "allow.h"

char rcs_info[] = "$Id: fcronsighup.c,v 1.1 2001-07-07 17:29:51 thib Exp $";

void usage(void);
void sig_daemon(void);
pid_t read_pid(void);

uid_t uid = 0;

/* used in temp_file() */
char *tmp_path = "/tmp/";

#ifdef DEBUG
char debug_opt = 1;       /* set to 1 if we are in debug mode */
#else
char debug_opt = 0;       /* set to 1 if we are in debug mode */
#endif

/* needed by log part : */
char *prog_name = NULL;
char foreground = 1;
pid_t daemon_pid = 0;


void
usage(void)
  /*  print a help message about command line options and exit */
{
    fprintf(stderr,
	    "fcronsighup "VERSION_QUOTED" - make fcron update its fcrontabs\n"
	    "Copyright 2000-2001 Thibault Godouet <fcron@free.fr>\n"
	    "This program is free software distributed WITHOUT ANY WARRANTY.\n"
            "See the GNU General Public License for more details.\n"
	    "\n"
	);

    fprintf(stderr, 
	    "fcronsighup [fcronconf]\n"
	    "  Signal fcron process using fcronconf configuration file\n"
	    "  (or default configuration file " ETC "/" FCRON_CONF").\n"
	    "\n"
	);
    
    exit(EXIT_ERR);

}

pid_t
read_pid(void)
    /* return fcron daemon's pid if running.
     * otherwise return 0 */
{
    FILE *fp = NULL;
    pid_t pid = 0;
    
    if ((fp = fopen(pidfile, "r")) != NULL) {
	fscanf(fp, "%d", (int *) &pid);    
	fclose(fp);
    }

    return pid;
}


void
sig_daemon(void)
    /* send SIGHUP to daemon to tell him configuration has changed */
    /* SIGHUP is sent once 10s before the next minute to avoid
     * some bad users to block daemon by sending it SIGHUP all the time */
{
    /* we don't need to make root wait */
    if (uid != 0) {
	time_t t = 0;
	int sl = 0;
	FILE *fp = NULL;
	int	fd = 0;
	struct tm *tm = NULL;
	char sigfile[PATH_LEN];
	char buf[FNAME_LEN];

	t = time(NULL);
	tm = localtime(&t);
    
	if ( (sl = 60 - (t % 60) - 10) < 0 ) {
	    if ( (tm->tm_min = tm->tm_min + 2) >= 60 ) {
		tm->tm_hour++;
		tm->tm_min -= 60;
	    }
	    snprintf(buf, sizeof(buf), "%02dh%02d", tm->tm_hour, tm->tm_min);
	    sl = 60 - (t % 60) + 50;
	} else {
	    if ( ++tm->tm_min >= 60 ) {
		tm->tm_hour++;
		tm->tm_min -= 60;
	    }
	    snprintf(buf, sizeof(buf), "%02dh%02d", tm->tm_hour, tm->tm_min);
	}
	fprintf(stderr, "Modifications will be taken into account"
		" at %s.\n", buf);

	snprintf(sigfile, sizeof(sigfile), "%s/fcrontab.sig", fcrontabs);

	switch ( fork() ) {
	case -1:
	    remove(sigfile);
	    die_e("could not fork : daemon has not been signaled");
	    break;
	case 0:
	    /* child */
	    break;
	default:
	    /* parent */
	    return;
	}

	foreground = 0;

	/* try to create a lock file */
	if ((fd = open(sigfile, O_RDWR|O_CREAT, 0644)) == -1
	    || ((fp = fdopen(fd, "r+")) == NULL) )
	    die_e("can't open or create %s", sigfile);	
    
#ifdef HAVE_FLOCK
	if ( flock(fd, LOCK_EX|LOCK_NB) != 0 ) {
	    debug("fcrontab is already waiting for signalling the daemon :"
		  " exiting.");
	    return;
	}
#else /* HAVE_FLOCK */
	if ( lockf(fd, F_TLOCK, 0) != 0 ) {
	    debug("fcrontab is already waiting for signalling the daemon :"
		  " exiting.");
	    return;
	}
#endif /* ! HAVE_FLOCK */

	sleep(sl);
    
	fclose(fp);
	close(fd);

	remove(sigfile);
    }
    else
	fprintf(stderr, "Modifications will be taken into account"
		" right now.\n");

    if ( (daemon_pid = read_pid()) == 0 )
	/* daemon is not running any longer : we exit */
	return ;

    foreground = 1;

    if ( kill(daemon_pid, SIGHUP) != 0)
	die_e("could not send SIGHUP to daemon (pid %d)", daemon_pid);

}


int
main(int argc, char **argv)
{
    struct passwd *pass;

    if (strrchr(argv[0],'/')==NULL) prog_name = argv[0];
    else prog_name = strrchr(argv[0],'/')+1;

    if ( argc == 2 )
	fcronconf = argv[1];
    else if (argc > 2 )
	usage();

    /* read fcron.conf and update global parameters */
    read_conf();
    
    uid = getuid();

    /* check if user is allowed to use this program */
    if ( ! (pass = getpwuid(uid)) )
	die("user \"%s\" is not in passwd file. Aborting.", USERNAME);

    if ( is_allowed(pass->pw_name) ) {
	/* check if daemon is running */
	if ( (daemon_pid = read_pid()) != 0 )
	    sig_daemon();
	else
	    fprintf(stderr, "fcron is not running :\n  modifications will"
		    " be taken into account at its next execution.\n");
    }
    else
	die("User \"%s\" is not allowed to use %s. Aborting.",
	    pass->pw_name, prog_name);

    return EXIT_OK;

}
