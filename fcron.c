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

 /* $Id: fcron.c,v 1.39 2001-01-27 15:33:39 thib Exp $ */

#include "fcron.h"

char rcs_info[] = "$Id: fcron.c,v 1.39 2001-01-27 15:33:39 thib Exp $";

void main_loop(void);
void check_signal(void);
void info(void);
void usage(void);
RETSIGTYPE sighup_handler(int x);
RETSIGTYPE sigterm_handler(int x);
RETSIGTYPE sigchild_handler(int x);
RETSIGTYPE sigusr1_handler(int x);
int parseopt(int argc, char *argv[]);
void get_lock(void);


/* command line options */
#ifdef DEBUG
char debug_opt = 1;       /* set to 1 if we are in debug mode */
#else
char debug_opt = 0;       /* set to 1 if we are in debug mode */
#endif

#ifdef FOREGROUND
char foreground = 1; /* set to 1 when we are on foreground, else 0 */
#else
char foreground = 0; /* set to 1 when we are on foreground, else 0 */
#endif

char  *cdir = FCRONTABS;      /* the dir where are stored users' fcrontabs */

/* process identity */
pid_t daemon_pid;               
char *prog_name = NULL;         

/* locking */
static FILE *daemon_lockfp = NULL;

/* have we got a signal ? */
char sig_conf = 0;            /* is 1 when we got a SIGHUP, 2 for a SIGUSR1 */ 
char sig_chld = 0;            /* is 1 when we got a SIGCHLD */  

/* jobs database */
struct CF *file_base;         /* point to the first file of the list */
struct job *queue_base;       /* ordered list of normal jobs to be run */

struct CL **serial_array;     /* ordered list of job to be run one by one */
short int serial_array_size;  /* size of serial_array */
short int serial_array_index; /* the index of the first job */
short int serial_num;         /* number of job being queued */
short int serial_running;     /* number of running serial jobs */

struct lavg *lavg_array;      /* jobs waiting for a given system load value */
short int lavg_array_size;    /* size of lavg_array */
short int lavg_num;           /* number of job being queued */
short int lavg_serial_running;/* number of serialized lavg job being running */

struct exe *exe_array;        /* jobs which are executed */
short int exe_array_size;     /* size of exe_array */
short int exe_num;            /* number of job being executed */

time_t begin_sleep;           /* the time at which sleep began */
time_t now;                   /* the current time */


void
info(void)
    /* print some informations about this program :
     * version, license */
{
    fprintf(stderr,
	    "fcron " VERSION_QUOTED " - periodic command scheduler\n"
	    "Copyright 2000-2001 Thibault Godouet <fcron@free.fr>\n"
	    "This program is free software distributed WITHOUT ANY WARRANTY.\n"
            "See the GNU General Public License for more details.\n"
	);

    exit(EXIT_OK);

}


void
usage()
  /*  print a help message about command line options and exit */
{
    fprintf(stderr, "\nfcron " VERSION_QUOTED "\n\n"
	    "fcron [-d] [-f] [-b]\n"
	    "fcron -h\n"
	    "  -d     --debug          Set Debug mode.\n"
	    "  -f     --foreground     Stay in foreground.\n"
	    "  -b     --background     Go to background.\n"
	    "  -h     --help           Show this help message.\n"
	);
    
    exit(EXIT_ERR);
}


void 
xexit(int exit_value) 
    /* exit after having freed memory and removed lock file */
{
    CF *f = NULL;

    now = time(NULL);

    /* we save all files now and after having waiting for all
     * job being executed because we might get a SIGKILL
     * if we don't exit quickly */
    save_file(NULL);
    
    f = file_base;
    while ( f != NULL ) {
	if ( f->cf_running > 0 ) {
	    wait_all( &f->cf_running );
	    save_file(f);
	}
	delete_file(f->cf_user);    

	/* delete_file remove the f file from the list :
	 * next file to remove is now pointed by file_base. */
	f = file_base;
    }

    remove(PIDFILE);
    
    explain("Exiting with code %d", exit_value);
    exit (exit_value);

}

void
get_lock()
    /* check if another fcron daemon is running : in this case, die.
     * if not, write our pid to /var/run/fcron.pid in order to lock,
     * and to permit fcrontab to read our pid and signal us */
{

    if ( ! daemon_lockfp ) {
	int	fd, otherpid, foreopt;

	foreopt = foreground;
	foreground = 1;

	if (((fd = open(PIDFILE, O_RDWR|O_CREAT, 0644)) == -1 )
	    || ((daemon_lockfp = fdopen(fd, "r+"))) == NULL)
	    die_e("can't open or create " PIDFILE);
	
	if ( flock(fd, LOCK_EX|LOCK_NB) != 0 ) {
	    if ((errno == EAGAIN) || (errno == EACCES))
		errno = EWOULDBLOCK;
	    fscanf(daemon_lockfp, "%d", &otherpid);
	    die("can't lock " PIDFILE ", running daemon's pid may be %d",
		otherpid);
	}

	(void) fcntl(fd, F_SETFD, 1);

	foreground = foreopt;

    }
    
    rewind(daemon_lockfp);
    fprintf(daemon_lockfp, "%d\n", daemon_pid);
    fflush(daemon_lockfp);
    (void) ftruncate(fileno(daemon_lockfp), ftell(daemon_lockfp));

    /* abandon fd and daemon_lockfp even though the file is open. we need to
     * keep it open and locked, but we don't need the handles elsewhere.
     */


}


int
parseopt(int argc, char *argv[])
  /* set options */
{

    char c;
    int i;

#ifdef HAVE_GETOPT_H
    static struct option opt[] =
    {
	{"debug",0,NULL,'d'},
	{"foreground",0,NULL,'f'},
	{"background",0,NULL,'b'},
	{"help",0,NULL,'h'},
	{"version",0,NULL,'V'},
	{0,0,0,0}
    };
#endif /* HAVE_GETOPT_H */

    extern char *optarg;
    extern int optind, opterr, optopt;

    /* constants and variables defined by command line */

    while(1) {
#ifdef HAVE_GETOPT_H
	c = getopt_long(argc, argv, "dfbhV", opt, NULL);
#else
	c = getopt(argc, argv, "dfbhV");
#endif /* HAVE_GETOPT_H */
	if (c == -1) break;
	switch (c) {

	case 'V':
	    info(); break;

	case 'h':
	    usage(); break;

	case 'd':
	    debug_opt = 1; break;

	case 'f':
	    foreground = 1; break;

	case 'b':
	    foreground = 0; break;

	case 'c':
	    cdir = optarg; break;

	case ':':
	    error("(parseopt) Missing parameter");
	    usage();

	case '?':
	    usage();

	default:
	    warn("(parseopt) Warning: getopt returned %c", c);
	}
    }

    if (optind < argc) {
	for (i = optind; i<=argc; i++)
	    error("Unknown argument \"%s\"", argv[i]);
	usage();
    }

    return OK;

}


RETSIGTYPE
sigterm_handler(int x)
  /* exit safely */
{
    debug("");
    explain("SIGTERM signal received");
    xexit(EXIT_OK);
}

RETSIGTYPE
sighup_handler(int x)
  /* update configuration */
{
    signal(SIGHUP, sighup_handler);
    siginterrupt(SIGHUP, 0);
    /* we don't call syslog() (by debug and explain) here anymore, because
     * it may cause a crash if the signal is received during another call
     * of syslog() */
/*      debug(""); */
/*      explain("SIGHUP signal received"); */
    /* we don't call the synchronize_dir() function directly,
       because it may cause some problems if this signal
       is not received during the sleep
    */
    sig_conf = 1;
}

RETSIGTYPE
sigchild_handler(int x)
  /* call wait_chld() to take care of finished jobs */
{
    /* we don't call syslog() (by debug and explain) here anymore, because
     * it may cause a crash if the signal is received during another call
     * of syslog() */
/*      debug(""); */
/*      debug("SIGCHLD signal received."); */
    
    sig_chld = 1;

    (void)signal(SIGCHLD, sigchild_handler);
    siginterrupt(SIGCHLD, 0);
}


RETSIGTYPE
sigusr1_handler(int x)
  /* reload all configurations */
{
    signal(SIGUSR1, sigusr1_handler);
    siginterrupt(SIGUSR1, 0);
    /* we don't call syslog() (by debug and explain) here anymore, because
     * it may cause a crash if the signal is received during another call
     * of syslog() */
/*      debug(""); */
/*      explain("SIGUSR1 signal received"); */
    /* we don't call the synchronize_dir() function directly,
       because it may cause some problems if this signal
       is not received during the sleep
    */
    sig_conf = 2;
}


int
main(int argc, char **argv)
{

    /* we set it to 022 in order to get a PIDFILE readable by fcrontab
     * (will be set to 066 later) */
    umask(022);

    /* parse options */

    if ( strrchr(argv[0], '/') == NULL) prog_name = argv[0];
    else prog_name = strrchr(argv[0], '/') + 1;

    {
	uid_t daemon_uid;                 
	if ( (daemon_uid = getuid()) != 0 )
	    die("Fcron must be executed as root");
    }

    /* we have to set daemon_pid before the fork because it's
     * used in die() and die_e() functions */
    daemon_pid = getpid();

    parseopt(argc, argv);

    /* change directory */

    if (chdir(cdir) != 0)
	die_e("Could not change dir to " FCRONTABS);
    

    if (foreground == 0) {

	/* close stdin, stdout and stderr.
	 * close unused descriptors
	 * optional detach from controlling terminal */

	int fd, i;
	pid_t pid;

	/* check if another fcron daemon is running */
	get_lock();

	switch ( pid = fork() ) {
	case -1:
	    die_e("fork");
	    break;
	case 0:
	    /* child */
	    break;
	default:
	    /* parent */
/*  	    printf("%s[%d] " VERSION_QUOTED " : started.\n", */
/*  		   prog_name, pid); */

	    exit(0);
	}

	daemon_pid = getpid();

	if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
	    ioctl(fd, TIOCNOTTY, 0);
	    close(fd);
	}

	if ( (i = open("/dev/null", O_RDWR)) < 0)
	    die_e("open: /dev/null:");
 	close(0); dup2(i, 0);
	close(1); dup2(i, 1);
	close(2); dup2(i, 2);

	/* close most other open fds */
	i = fileno(daemon_lockfp);
	for(fd = 3; fd < 250; fd++) if (fd != i) (void) close(fd);

	/* finally, create a new session */
	if ( setsid() == -1 )
	    error("Could not setsid()");

    }

    /* if we are in foreground, check if another fcron daemon
     * is running, otherwise update value of pid in lock file */
    get_lock();
    
    /* become USERNAME : this is needed to be able to be signaled
     * by fcrontab */
#if defined(HAVE_SETREGID) && defined(HAVE_SETREUID)
    {
	struct passwd *pass;

	if ( ! (pass = getpwnam(USERNAME)) )
	    die("user \"%s\" is not in passwd file. Aborting.", USERNAME);
	if (setregid(pass->pw_gid, 0) != 0 )
	    die_e("Could not set gid to " GROUPNAME);
	if (setreuid(pass->pw_uid, 0) != 0 )
	    die_e("Could not set uid to " USERNAME);
    }
#endif

    /* this program belongs to root : we set default permission mode
     * to  600 for security reasons */
    umask(066);

    explain("%s[%d] " VERSION_QUOTED " started", prog_name, daemon_pid);

    signal(SIGTERM, sigterm_handler);
    signal(SIGHUP, sighup_handler);
    siginterrupt(SIGHUP, 0);
    signal(SIGCHLD, sigchild_handler);
    siginterrupt(SIGCHLD, 0);
    signal(SIGUSR1, sigusr1_handler);
    siginterrupt(SIGUSR1, 0);


    /* initialize exe_array */
    exe_num = 0;
    exe_array_size = EXE_INITIAL_SIZE;
    if ( (exe_array = calloc(exe_array_size, sizeof(struct exe))) == NULL )
	die_e("could not calloc exe_array");

    /* initialize serial_array */
    serial_running = 0;
    serial_array_index = 0;
    serial_num = 0;
    serial_array_size = SERIAL_INITIAL_SIZE;
    if ( (serial_array = calloc(serial_array_size, sizeof(CL *))) == NULL )
	die_e("could not calloc serial_array");

    /* initialize lavg_array */
    lavg_num = 0;
    lavg_serial_running = 0;
    lavg_array_size = LAVG_INITIAL_SIZE;
    if ( (lavg_array = calloc(lavg_array_size, sizeof(lavg))) == NULL )
	die_e("could not calloc lavg_array");



    main_loop();

    /* never reached */
    return EXIT_OK;
}


void
check_signal()
    /* check if a signal has been received and handle it */
{

    if (sig_chld > 0) {
	wait_chld();
	sig_chld = 0;
    }
    if (sig_conf > 0) {

	if (sig_conf == 1)
	    /* update configuration */
	    synchronize_dir(".");
	else
	    /* reload all configuration */
	    reload_all(".");

	sig_conf = 0;
    }

}

void
main_loop()
  /* main loop - get the time to sleep until next job execution,
   *             sleep, and then test all jobs and execute if needed. */
{
    time_t save;           /* time remaining until next save */
    struct timeval tv;     /* we use usec field to get more precision */
    time_t stime;          /* time to sleep until next job
			    * execution */

    debug("Entering main loop");

    now = time(NULL);

    synchronize_dir(".");

    /* synchronize save with jobs execution */
    save = now + SAVE;

    if ( serial_num > 0 )
	stime = FIRST_SLEEP;
    else if ( (stime = time_to_sleep(save)) < FIRST_SLEEP )
	/* force first execution after FIRST_SLEEP sec : execution of jobs
	 * during system boot time is not what we want */
	stime = FIRST_SLEEP;
    

    for (;;) {
	
#ifdef HAVE_GETTIMEOFDAY
	sleep(stime - 1);
	gettimeofday(&tv, NULL);
	usleep( 1000000 - tv.tv_usec );
#else
	sleep(stime);
#endif /* HAVE_GETTIMEOFDAY */

	now = time(NULL);

	check_signal();

	debug("\n");
	test_jobs();

    	if ( serial_running <= 0)
     	    run_serial_job();

	if ( save <= now ) {
	    save = now + SAVE;
	    /* save all files */
	    save_file(NULL);
	}

	stime = check_lavg(save);
	debug("sleep time : %ld", stime);

	check_signal();

    }

}
