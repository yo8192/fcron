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

 /* fcron.c */

#include "fcron.h"

void main_loop(void);
void info(void);
void usage(void);
void xexit(int exit_value);
void sighup_handler(int x);
void sigterm_handler(int x);
void sigchild_handler(int x);
void sigusr1_handler(int x);
int parseopt(int argc, char *argv[]);
void get_lock(void);


char debug_opt = DEBUG;       /* set to 1 if we are in debug mode */
char foreground = FOREGROUND; /* set to 1 when we are on foreground, else 0 */
char  *cdir = FCRONTABS;       /* the dir where are stored users' fcrontabs */
int daemon_uid;                 
pid_t daemon_pid;               
char *prog_name = NULL;         
char sig_conf = 0;             /* is 1 when we got a SIGHUP */ 
char sig_chld = 0;            /* is 1 when we got a SIGCHLD */  
CF *file_base;                /* point to the first file of the list */
int jobs_running = 0;         /* number of jobs which are running */
time_t t1;                    /* the time at which sleep began */


void
info(void)
    /* print some informations about this program :
     * version, license */
{
    fprintf(stderr,
	    "fcron " VERSION " - periodic command scheduler\n"
	    "Copyright 2000 Thibault Godouet <sphawk@free.fr>\n"
	    "This program is free software; you can redistribute it\n"
	    " and/or modify it under the terms of the GNU General Public\n"
	    " License. See file LICENSE distributed with this program\n"
	    " for more informations\n"
	);

    exit(EXIT_OK);

}


void
usage()
  /*  print a help message about command line options and exit */
{
    fprintf(stderr, "\nfcron " VERSION "\n\n"
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
    extern time_t t1;
    time_t t2 = time(NULL);
    time_t dt = 0;
    
    dt = t2 - t1;

    if (dt > 0) {

	debug("slept: %lds", dt);
	
	update_time_remaining(dt);
    }

    /* we save all files now and after having waiting for all
     * job being executed because we might get a SIGKILL
     * if we don't exit quickly */
    save_file(NULL, NULL);

    f = file_base;
    while ( f != NULL ) {
	if ( f->cf_running > 0 ) {
	    wait_all( &f->cf_running );
	    save_file(f, NULL);
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
    static FILE *fp = NULL;

    if ( ! fp ) {
	int	fd, otherpid, foreopt;

	foreopt = foreground;
	foreground = 1;

	if (((fd = open(PIDFILE, O_RDWR|O_CREAT, 0644)) == -1 )
	    || ((fp = fdopen(fd, "r+"))) == NULL)
	    die_e("can't open or create " PIDFILE);
	
    
	if ( flock(fd, LOCK_EX|LOCK_NB) != 0 ) {
	    if ((errno == EAGAIN) || (errno == EACCES))
		errno = EWOULDBLOCK;
	    fscanf(fp, "%d", &otherpid);
	    die("can't lock " PIDFILE ", running daemon's pid may be %d",
		otherpid);
	}

	(void) fcntl(fd, F_SETFD, 1);

	foreground = foreopt;

    }

    rewind(fp);
    fprintf(fp, "%d\n", daemon_pid);
    fflush(fp);
    (void) ftruncate(fileno(fp), ftell(fp));

    /* abandon fd and fp even though the file is open. we need to
     * keep it open and locked, but we don't need the handles elsewhere.
     */


}


int
parseopt(int argc, char *argv[])
  /* set options */
{

    char c;
    int i;
    static struct option opt[] =
    {
	{"debug",0,NULL,'d'},
	{"foreground",0,NULL,'f'},
	{"background",0,NULL,'b'},
	{"help",0,NULL,'h'},
	{"version",0,NULL,'V'},
	{0,0,0,0}
    };
    extern char *optarg;
    extern int optind, opterr, optopt;

    /* constants and variables defined by command line */

    while(1) {
	c = getopt_long(argc, argv, "dfbhV", opt, NULL);
	if (c == -1) break;
	switch (c) {

	case 'V':
	    info(); break;

	case 'h':
	    usage(); break;

	case 'd':
	    debug_opt = 1;
	    foreground = 1;

	case 'f':
	    foreground = 1; break;

	case 'b':
	    foreground = 0; break;

	case 'c':
	    cdir = optarg; break;

	case ':':
	    error("(setopt) Missing parameter");
	    usage();

	case '?':
	    usage();

	default:
	    warn("(setopt) Warning: getopt returned %c", c);
	}
    }

    if (optind < argc) {
	for (i = optind; i<=argc; i++)
	    error("Unknown argument '%s'", argv[i]);
	usage();
    }

    return OK;

}


void
sigterm_handler(int x)
  /* exit safely */
{
    debug("");
    explain("SIGTERM signal received");
    xexit(EXIT_OK);
}

void
sighup_handler(int x)
  /* update configuration */
{
    signal(SIGHUP, sighup_handler);
    debug("");
    explain("SIGHUP signal received");
    /* we don't call the synchronize_dir() function directly,
       because it may cause some problems if this signal
       is not received during the sleep
    */
    sig_conf = 1;
}

void
sigchild_handler(int x)
  /* call wait_chld() to take care of finished jobs */
{
    debug("");
    debug("SIGCHLD signal received.");
    
    sig_chld = 1;

    (void)signal(SIGCHLD, sigchild_handler);
}


void
sigusr1_handler(int x)
  /* reload all configurations */
{
    signal(SIGUSR1, sigusr1_handler);
    debug("");
    explain("SIGUSR1 signal received");
    /* we don't call the synchronize_dir() function directly,
       because it may cause some problems if this signal
       is not received during the sleep
    */
    sig_conf = 2;
}


int
main(int argc, char **argv)
{
    int i;

    /* this program belongs to root : we set default permission mode
     * to  600 for security reasons */
    umask(066);

    /* parse options */

    if (strrchr(argv[0],'/')==NULL) prog_name = argv[0];
    else prog_name = strrchr(argv[0],'/')+1;

    daemon_uid = getuid();

    /* we have to set daemon_pid before the fork because it's
     * used in die() and die_e() functions */
    daemon_pid = getpid();

    parseopt(argc, argv);

    /* change directory */

    if (chdir(cdir) != 0)
	die_e("Could not change dir to " FCRONTABS);
    

    if (foreground == 0) {

	/*
	 * close stdin and stdout (stderr normally redirected by caller).
	 * close unused descriptors
	 * optional detach from controlling terminal
	 */

	int fd;
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
	    printf("\n%s[%d] " VERSION " : started.\n\n",
		   prog_name, pid);

	    exit(0);
	}

	daemon_pid = getpid();

	if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
	    ioctl(fd, TIOCNOTTY, 0);
	    close(fd);
	}

	fclose(stdin);
	fclose(stdout);

	if ( (i = open("/dev/null", O_RDWR)) < 0)
	    die_e("open: /dev/null:");
	dup2(i, 0);
	dup2(i, 1);


	if(debug_opt) {
	    /* wait until child death and log his return value
	     * on error */
	    int status;
		
	    switch ( pid = fork() ) {
	    case -1:
		die_e("fork");
		break;
	    case 0:
		/* child */
		daemon_pid = getpid();
		break;
	    default:
		/* parent */
		while ( wait4(pid, &status, 0, NULL) != pid ) ;
		if ( ! WIFEXITED(status) )
		    error("fcron[%d] has exited with status %d",
			  pid, WEXITSTATUS(status));
		if ( WIFSIGNALED(status) )
		    error("fcron[%d] has exited due to signal %d",
			  pid, WTERMSIG(status));

		exit(0);

	    }
	}


    }

    /* if we are in foreground, check if another fcron daemon
     * is running, otherwise update value of pid in lock file */
    get_lock();
    
    explain("%s[%d] " VERSION " started", prog_name, daemon_pid);

    (void)signal(SIGTERM, sigterm_handler);
    (void)signal(SIGHUP, sighup_handler);
    (void)signal(SIGCHLD, sigchild_handler);
    (void)signal(SIGUSR1, sigusr1_handler);

    main_loop();

    /* never reached */
    return EXIT_OK;
}


void main_loop()
  /* main loop - get the time to sleep until next job execution,
   *             sleep, and then test all jobs and execute if needed. */
{
    extern time_t t1;     /* time at the beginning of sleeping */
    time_t t2 = 0;        /* time at the end of sleeping */
    time_t t3 = 0;        /* time at the previous reception of SIGCHLD */
    long int dt = 0;      /* time we slept */
    time_t save = SAVE;   /* time remaining until next save */
    struct timeval tv;    /* we use usec field to get more precision */
    time_t stime = 0;     /* time to sleep until next job
		           * execution */

    debug("Entering main loop");

    t1 = time(NULL);

    synchronize_dir(".");

    /* synchronize save with jobs execution */
    save += (60 - (t1 % 60));
    stime = time_to_sleep(save);

    for (;;) {

	sig_chld = 0;

      sleep:
	sleep(stime - 1);
	gettimeofday(&tv, NULL);
	usleep( 1000000 - tv.tv_usec );

	if (sig_chld > 0) {

	    /* sleep has been stopped too early :
	     * sleep the remaining time */
	    wait_chld();
	    sig_chld = 0;

	    if ( t3 < t1 ) t3 = t1;
	    dt = time(NULL) - t3;

	    if ( dt > 0 ) {
		/* update stime value */
		dt = stime - dt;
		debug("remain %d s of sleeping", dt);
		stime = dt;
		t3 = time(NULL);

	    } else
		/* we have slept less than 1 second : old stime
		 * is still valid */
		;

	    goto sleep;

	}

	t2 = time(NULL);
	dt = t2 - t1;

	if (dt > 0) {

	    debug("\n");
	    debug("slept: %lds", dt);

	    update_time_remaining(dt);

	    if (sig_conf > 0) {

		if (sig_conf == 1)
		    /* update configuration */
		    synchronize_dir(".");
		else {
		    /* reload all configuration */
		    update_time_remaining(dt);		    
		    reload_all(".");
		}		
		sig_conf = 0;
	    }

	    test_jobs(t2);

	    if ( (save = ( (save-dt) >= 0 ) ? save-dt : 0) == 0) {
		save = SAVE - (t2 % 60);
		/* save all files */
		save_file(NULL, NULL);
	    }

	}

	if ( sig_chld == 1 )
	    wait_chld();

	stime = time_to_sleep(save);

	t1 = t2;
    }

}
