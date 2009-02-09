/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2009 Thibault Godouet <fcron@free.fr>
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

 /* $Id: fcrondyn.c,v 1.19 2007-04-14 18:04:11 thib Exp $ */

/* fcrondyn : interact dynamically with running fcron process :
 *     - list jobs, with their status, next time of execution, etc
 *     - run, delay a job
 *     - send a signal to a job
 *     - etc ...
 */

#include "fcrondyn.h"
#include "allow.h"
#include "read_string.h"

char rcs_info[] = "$Id: fcrondyn.c,v 1.19 2007-04-14 18:04:11 thib Exp $";

void info(void);
void usage(void);
void xexit(int exit_val);
RETSIGTYPE sigpipe_handler(int x);
int interactive_mode(int fd);
/* returned by parse_cmd() and/or talk_fcron() */
#define QUIT_CMD 1
#define HELP_CMD 2
#define ZEROLEN_CMD 3
#define CMD_NOT_FOUND 4
#define INVALID_ARG 5
int talk_fcron(char *cmd_str, int fd);
int parse_cmd(char *cmd_str, long int **cmd, int *cmd_len);
int connect_fcron(void);
int authenticate_user_password(int fd);

/* command line options */
#ifdef DEBUG
char debug_opt = 1;       /* set to 1 if we are in debug mode */
#else
char debug_opt = 0;       /* set to 1 if we are in debug mode */
#endif
char *cmd_str = NULL;

/* needed by log part : */
char *prog_name = NULL;
char foreground = 1;
char dosyslog = 1;
pid_t daemon_pid = 0;

/* uid/gid of user/group root 
 * (we don't use the static UID or GID as we ask for user and group names
 * in the configure script) */
uid_t rootuid = 0;
gid_t rootgid = 0;

/* misc */
char *user_str;
uid_t user_uid;

/* if you change this structure, please update NUM_CMD value in dyncom.h */
struct cmd_list_ent cmd_list[NUM_CMD] = {
    /* name, desc, num opt, cmd code, cmd opts, cmd defaults */
    {"ls", "List all jobs of user", 1, CMD_LIST_JOBS,
     {USER}, {CUR_USER} },
    {"ls_lavgq", "List jobs of user which are in lavg queue", 1, CMD_LIST_LAVGQ,
     {USER}, {CUR_USER}},
    {"ls_serialq", "List jobs of user which are in serial queue", 1, CMD_LIST_SERIALQ,
     {USER}, {CUR_USER}},
    {"ls_exeq", "List running jobs of user", 1, CMD_LIST_EXEQ,
     {USER}, {CUR_USER}},
    {"detail", "Print details on job", 1, CMD_DETAILS, 
     {JOBID}, {ARG_REQUIRED}},
/*    {"reschedule", "Reschedule next execution of job", 2, CMD_RESCHEDULE,
      {TIME_AND_DATE, JOBID}, {ARG_REQUIRED, ARG_REQUIRED}}, */
    {"runnow", "Advance next execution of job to now", 1, CMD_RUNNOW,
     {JOBID}, {ARG_REQUIRED}},
    {"run", "Run job now (without changing its current schedule)", 1, CMD_RUN,
     {JOBID}, {ARG_REQUIRED}},
    {"kill", "Send signal to running job", 2, CMD_SEND_SIGNAL,
     {SIGNAL, JOBID}, {ARG_REQUIRED, ARG_REQUIRED}},
    {"renice", "Renice running job", 2, CMD_RENICE,
     {NICE_VALUE, JOBID}, {ARG_REQUIRED, ARG_REQUIRED}}
};


void
info(void)
    /* print some informations about this program :
     * version, license */
{
    fprintf(stderr,
	    "fcrondyn " VERSION_QUOTED " - interact dynamically with daemon fcron\n"
	    "Copyright " COPYRIGHT_QUOTED " Thibault Godouet <fcron@free.fr>\n"
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
	    "fcrondyn [-i]\n"
	    "fcrondyn -x 'command'\n"
	    "fcrondyn -h\n"
	    "  -c f       make fcrontab use config file f.\n"
	    "  -d         set up debug mode.\n"
	    "  -h         display this help message.\n"
	    "  -V         display version & infos about fcrondyn.\n"
	    "\n"
	);
    
    exit(EXIT_ERR);
}

RETSIGTYPE
sigpipe_handler(int x)
    /* handle broken pipes ... */
{
    fprintf(stderr, "Broken pipe : fcron may have closed the connection\nThe connection "
	    "has been idle for more than %ds, or fcron may not be running anymore.\n",
	    MAX_IDLE_TIME);
    fprintf(stderr, "Exiting ...\n");

    xexit(EXIT_ERR);
}

void
xexit(int exit_val)
    /* clean & exit */
{
    Flush(cmd_str);

    exit(exit_val);
}


/* used in parse_cmd : */
#define Write_cmd(DATA) \
	  memcpy(buf + *cmd_len, &DATA, sizeof(long int)); \
	  *cmd_len += 1;

#define Strncmp(STR1, STR2, STR1_SIZE) \
          strncmp(STR1, STR2, (STR1_SIZE < strlen(STR2)) ? strlen(STR2) : STR1_SIZE)

int
parse_cmd(char *cmd_str, long int **cmd, int *cmd_len)
    /* read a command string, check if it is valid and translate it */
{
    long int buf[SOCKET_MSG_LEN];
    int word_size = 0;
    int i = 0, j = 0, rank = -1;
    long int int_buf = 0;
    struct passwd *pass = NULL;
#ifdef SYSFCRONTAB
    long int sysfcrontab_uid = SYSFCRONTAB_UID;
#endif

    bzero(buf, sizeof(buf));
    *cmd_len = 0;
    remove_blanks(cmd_str); /* at the end of the string */
    
    if ( (word_size = get_word(&cmd_str)) == 0 ) {
	fprintf(stderr, "Warning : Zero-length command name : line ignored.\n");
	return ZEROLEN_CMD;
    }

    if (Strncmp(cmd_str, "q", word_size) == 0 || Strncmp(cmd_str, "quit", word_size) == 0
	|| Strncmp(cmd_str, "exit", word_size) == 0) {
	if ( debug_opt ) 
	    fprintf(stderr, "quit command\n");
	return QUIT_CMD;
    }

    if( Strncmp(cmd_str, "h", word_size)==0 || Strncmp(cmd_str, "help", word_size)==0 ) {
	if ( debug_opt ) 
	    fprintf(stderr, "help command\n");
	return HELP_CMD;
    }

    for (i = 0; i < NUM_CMD; i++) {
	if ( Strncmp(cmd_str, cmd_list[i].cmd_name, word_size) == 0 ) {
	    rank = i;
	    break;
	}
    }
    if ( rank == (-1) ) {
	fprintf(stderr, "Error : Unknown command.\n");
	return CMD_NOT_FOUND;
    }
    Write_cmd(cmd_list[rank].cmd_code);

    if ( debug_opt )
	fprintf(stderr, "command : %s\n", cmd_list[i].cmd_name);

    cmd_str += word_size;
    for (i = 0 ; i < cmd_list[rank].cmd_numopt; i++) {

	if ( (word_size = get_word(&cmd_str)) == 0 ) {

	    if (cmd_list[rank].cmd_default[i] == ARG_REQUIRED) {
		fprintf(stderr, "Error : arg required !\n");
		return INVALID_ARG;
	    }

	    /* use default value : currently, works only with CUR_USER */
	    if ( user_uid == rootuid ) {
		/* default for root = all */
		int_buf = ALL;
		Write_cmd( int_buf );
		if ( debug_opt )
		    fprintf(stderr, "  uid = ALL\n");
	    }
	    else {
		Write_cmd( user_uid );
		if ( debug_opt )
		    fprintf(stderr, "  uid = %d\n", (int)user_uid);
	    }

	}

	else {
	    
	    /* get value from line ... */
	    switch (cmd_list[rank].cmd_opt[i]) {

	    case USER:
		int_buf = (long int) *(cmd_str + word_size);
		*(cmd_str + word_size) = '\0';
#ifdef SYSFCRONTAB
		if ( strcmp(cmd_str, SYSFCRONTAB) == 0 ) {
		    Write_cmd(sysfcrontab_uid);
		}
		else {
#endif
		    if ( ( pass = getpwnam(cmd_str) ) == NULL ) {
			fprintf(stderr,"Error : '%s' isn't a valid username.\n",cmd_str);
			return INVALID_ARG;
		    }
		    Write_cmd(pass->pw_uid);
#ifdef SYSFCRONTAB
		}
#endif
		*(cmd_str + word_size) = (char) int_buf;
		cmd_str += word_size;
		if ( debug_opt )
		    fprintf(stderr, "  uid = %d\n",
#ifdef SYSFCRONTAB
			    (pass) ? (int)pass->pw_uid : (int)SYSFCRONTAB_UID
#else
			    (int)pass->pw_uid
#endif
			);
		break;

	    case JOBID:
		/* after strtol(), cmd_str will be updated (first non-number char) */
		if ( (int_buf = strtol(cmd_str, &cmd_str, 10)) < 0 || int_buf >= LONG_MAX
		     || (! isspace( (int) *cmd_str) && *cmd_str != '\0') ) {
		    fprintf(stderr, "Error : invalid jobid.\n");
		    return INVALID_ARG;
		}
		Write_cmd(int_buf);
		if ( debug_opt )
		    fprintf(stderr, "  jobid = %ld\n", int_buf);
		break;

	    case TIME_AND_DATE:
		/* argghh !!! no standard function ! */
		break;

	    case NICE_VALUE:
		/* after strtol(), cmd_str will be updated (first non-number char) */
		if ( (int_buf = strtol(cmd_str, &cmd_str, 10)) > 20 
		     || (int_buf < 0 && getuid() != rootuid) || int_buf < -20
		     || (! isspace( (int) *cmd_str) && *cmd_str != '\0') ) {
		    fprintf(stderr, "Error : invalid nice value.\n");
		    return INVALID_ARG;
		}
		Write_cmd(int_buf);
		if ( debug_opt )
		    fprintf(stderr, "  nicevalue = %ld\n", int_buf);
		break;

	    case SIGNAL:
		if ( isalpha( (int) *cmd_str ) ) {
		    for (j = 0; j < word_size; j++)
			*(cmd_str+j) = tolower ( *(cmd_str+j) ); 
		    if ( Strncmp(cmd_str, "hup", word_size) == 0 ) int_buf = SIGHUP;
		    else if (Strncmp(cmd_str, "int", word_size) == 0) int_buf = SIGINT;
		    else if (Strncmp(cmd_str, "quit", word_size) == 0) int_buf = SIGQUIT;
		    else if (Strncmp(cmd_str, "kill", word_size) == 0) int_buf = SIGKILL;
		    else if (Strncmp(cmd_str, "alrm", word_size) == 0) int_buf = SIGALRM;
		    else if (Strncmp(cmd_str, "term", word_size) == 0) int_buf = SIGTERM;
		    else if (Strncmp(cmd_str, "usr1", word_size) == 0) int_buf = SIGUSR1;
		    else if (Strncmp(cmd_str, "usr2", word_size) == 0) int_buf = SIGUSR2;
		    else if (Strncmp(cmd_str, "cont", word_size) == 0) int_buf = SIGCONT;
		    else if (Strncmp(cmd_str, "stop", word_size) == 0) int_buf = SIGSTOP;
		    else if (Strncmp(cmd_str, "tstp", word_size) == 0) int_buf = SIGTSTP;
		    else {
			fprintf(stderr, "Error : unknow signal (try integer value)\n");
			return INVALID_ARG;
		    }
		    cmd_str += word_size;
		}
		/* after strtol(), cmd_str will be updated (first non-number char) */
		else if((int_buf=strtol(cmd_str, &cmd_str, 10)) <= 0 || int_buf>=LONG_MAX
		     || (! isspace( (int) *cmd_str) && *cmd_str != '\0') ) {
		    fprintf(stderr, "Error : invalid signal value.\n");
		    return INVALID_ARG;
		}
		Write_cmd(int_buf);
		if ( debug_opt )
		    fprintf(stderr, "  signal = %ld\n", int_buf);
		break;

	    default:
		fprintf(stderr, "Error : Unknown arg !");
		return INVALID_ARG;
	    }
	}
    }

    Skip_blanks(cmd_str);
    if ( *cmd_str != '\0' )
	fprintf(stderr, "Warning : too much arguments : '%s' ignored.\n", cmd_str);

    /* This is a valid command ... */
    if ( (*cmd = calloc(*cmd_len, sizeof(long int))) == NULL )
	die_e("Could not calloc.");

    memcpy(*cmd, buf, *cmd_len * sizeof(long int));

    return OK;
}

int
authenticate_user_password(int fd)
    /* authenticate user */
{
    char *password = NULL;
    char buf[USER_NAME_LEN + 16];
    int len = 0;
    fd_set read_set; /* needed to use select to check if some data is waiting */
    struct timeval tv;

    snprintf(buf, sizeof(buf), "password for %s :", user_str);
    if ( (password = read_string(CONV_ECHO_OFF, buf)) == NULL )
	return ERR;

    len = snprintf(buf, sizeof(buf), "%s", user_str) + 1;
    len += snprintf(buf + len, sizeof(buf) - len, "%s", password) + 1;
    send(fd, buf, len, 0);
    Overwrite(buf);
    Overwrite(password);
    free(password);
    
    tv.tv_sec = MAX_WAIT_TIME;
    tv.tv_usec = 0;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    if ( select(fd + 1, &read_set, NULL, NULL, &tv) <= 0 ) {
	error_e("Couldn't get data from socket during %d seconds.", MAX_WAIT_TIME);
	return ERR;
    }
    while ( recv(fd, buf, sizeof(buf), 0) < 0 && errno == EINTR )
	if ( errno == EINTR && debug_opt )
	    fprintf(stderr, "Got EINTR ...");
    if ( strncmp(buf, "1", sizeof("1")) != 0 )
	return ERR;

    return OK;
}


int
connect_fcron(void)
    /* connect to fcron through a socket, and identify user */
{
    int fd = -1;
    struct sockaddr_un addr;
    int len = 0;
    int sun_len = 0;

    if ( (fd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1 )
	die_e("could not create socket");

    addr.sun_family = AF_UNIX;
    len = strlen(fifofile);
    if ( len > sizeof(addr.sun_path) - 1 )
	die("Error : fifo file path too long (max is %d)", sizeof(addr.sun_path) - 1);
    /* length(fifofile) < sizeof(add.sun_path), so strncpy will terminate
     * the string with at least one \0 (not necessarily required by the OS,
     * but still a good idea) */
    strncpy(addr.sun_path, fifofile, sizeof(addr.sun_path));
    addr.sun_path[sizeof(addr.sun_path)-1] = '\0';
    sun_len = (addr.sun_path - (char *)&addr) + len;
#if HAVE_SA_LEN
    addr.sun_len = sun_len;
#endif

    if ( connect(fd, (struct sockaddr *) &addr, sun_len) < 0 )
	die_e("Cannot connect() to fcron (check if fcron is running)");

/* Nothing to do on the client side if we use SO_PASSCRED */
#ifndef SO_PASSCRED
    if ( authenticate_user_password(fd) == ERR ) {
        fprintf(stderr, "Invalid password or too many authentication failures"
	    " (try to connect later).\n(In the later case, fcron rejects all"
	    " new authentication during %d secs)\n", AUTH_WAIT);	
	die("Unable to authenticate user");
    }
#endif /* SO_PASSCRED */

    return fd;

}

int
talk_fcron(char *cmd_str, int fd)
    /* read a string command, check if it is valid and translate it,
     * send it to fcron, and print its answer */
{
    long int *cmd = NULL;
    int cmd_len = 0;
    char buf[LINE_LEN];
    size_t read_len = 0;
    char existing_connection = (fd < 0) ? 0 : 1;
    fd_set read_set; /* needed to use select to check if some data is waiting */
    struct timeval tv;

    switch ( parse_cmd(cmd_str, &cmd, &cmd_len) ) {
    case OK:
	break;
    case HELP_CMD:
    {
	int i, j, len;
	printf("Command recognized by fcrondyn :\n");
	printf("------------------------------\n");
	for (i = 0; i < NUM_CMD; i++) {
	    len = printf("%s ", cmd_list[i].cmd_name);

	    /* print args : */
	    for (j = 0; j < cmd_list[i].cmd_numopt; j++) {
		if ( cmd_list[i].cmd_default[j] != ARG_REQUIRED )
		    len += printf("[");
		switch ( cmd_list[i].cmd_opt[j] ) {
		case USER: len += printf("user"); break;
		case JOBID: len += printf("jobid"); break;
		case TIME_AND_DATE: len += printf("time"); break;
		case NICE_VALUE: len += printf("niceval"); break;
		case SIGNAL: len += printf("sig"); break;
		case BOOLEAN: len += printf("bool"); break;
		default: len += printf("unknown_arg!");
		}
		if ( cmd_list[i].cmd_default[j] != ARG_REQUIRED )
		    len += printf("]");
		len += printf(" ");
	    }
	    /* Align correctly the descriptions : */
	    printf("%*s%s\n", 24 - len, "",  cmd_list[i].cmd_desc);
	}
	printf("\n");
	printf("help\t\t\tDisplay this help message\n");
	printf("quit\t\t\tQuit fcrondyn\n");
    }
	return HELP_CMD;
    case QUIT_CMD:
	return QUIT_CMD;
    case CMD_NOT_FOUND:
	return CMD_NOT_FOUND;
    case INVALID_ARG:
	return INVALID_ARG;
    case ZEROLEN_CMD:
	return ZEROLEN_CMD;
    default:
	return ERR;
    }

    /* This is a valid command (so we'll have to free() it) ... */

    if ( ! existing_connection && (fd = connect_fcron()) == ERR )
	return ERR;

    send(fd, cmd, cmd_len * sizeof(long int), 0);
    Flush(cmd);
    cmd_len = 0;

    tv.tv_sec = MAX_WAIT_TIME;
    tv.tv_usec = 0;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    if ( select(fd + 1, &read_set, NULL, NULL, &tv) <= 0 ) {
	error_e("Couldn't get data from socket during %d seconds.", MAX_WAIT_TIME);
	return ERR;
    }

    while ( (read_len = (size_t)recv(fd, buf, sizeof(buf), 0)) >= 0 || errno == EINTR ) {
	if ( errno == EINTR && debug_opt)
	    fprintf(stderr, "got EINTR ...\n");
	else if ( read_len > sizeof(buf) ) {
	    /* weird ... no data yet ? */
	    if ( debug_opt )
		fprintf(stderr, "no data yet ?");
	}
	else if ( read_len <= 0 ) {
	    if ( debug_opt )
		fprintf(stderr, "read_len = %d\n", (int)read_len);
	    fprintf(stderr, "connection closed by fcron\n");
	    shutdown(fd, SHUT_RDWR);
	    return ERR;
	}
	else {
	    if ( write(STDOUT_FILENO, buf, read_len) < 0 )
		error_e("unable to write() to STDOUT_FILENO");
	    if ( read_len >= sizeof(END_STR) &&
		 strncmp(&buf[read_len-sizeof(END_STR)], END_STR, sizeof(END_STR)) == 0)
		break;
	}
    }
    if ( read_len < 0 )
	error_e("error in recv()"); 

    if ( ! existing_connection )
	close(fd);

    return OK;
}


int
interactive_mode(int fd)
    /* provide user a prompt, read command, send it to fcron, print its answer,
     * then give another prompt, etc, until user type an exit command */
{
    char existing_connection = (fd < 0) ? 0 : 1;
    int return_code = 0;
    char buf[LINE_LEN];

    if ( ! existing_connection && (fd = connect_fcron()) == ERR )
	return ERR;

    while (fprintf(stderr, "fcrondyn> ") && fgets(buf, sizeof(buf), stdin) != NULL 
	   && (return_code = talk_fcron(buf, fd)) != QUIT_CMD && return_code != ERR ) ;

    if ( ! existing_connection )
	close(fd);

    return OK;
}


void
parseopt(int argc, char *argv[])
  /* set options */
{

    int c, i;
    extern char *optarg;
    extern int optind, opterr, optopt;

    /* constants and variables defined by command line */

    while(1) {
	c = getopt(argc, argv, "hVdc:ix:");
	if (c == EOF) break;
	switch (c) {

	case 'V':
	    info(); break;

	case 'h':
	    usage(); break;

	case 'd':
	    debug_opt = 1; break;

	case 'c':
	    Set(fcronconf, optarg);
	    break;

	case 'i':
	    Flush(cmd_str);
	    break;

	case 'x':
	    Set(cmd_str, optarg);
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

    if (optind < argc) {
	for (i = optind; i <= argc; i++)
	    fprintf(stderr, "Unknown argument \"%s\"", argv[i]);
	usage();
    }
}


int
main(int argc, char **argv)
{
    int return_code = 0;
    int fd = (-1);
    struct passwd *pass = NULL;

    rootuid = get_user_uid_safe(ROOTNAME);
    rootgid = get_group_gid_safe(ROOTGROUP);

    if ( strrchr(argv[0], '/') == NULL) prog_name = argv[0];
    else prog_name = strrchr(argv[0], '/') + 1;

    /* interpret command line options */
    parseopt(argc, argv);

/*      if (debug_opt) */
/*  	fprintf(stderr, "uid : %d euid : %d\n", getuid(), geteuid()); */

    /* read fcron.conf and update global parameters */
    read_conf();

    user_uid = getuid();
    if ( (pass = getpwuid(user_uid)) == NULL )
	die("user \"%s\" is not in passwd file. Aborting.", USERNAME);
    user_str = strdup2(pass->pw_name);

    /* we don't need anymore special rights : drop them */
    seteuid(user_uid);
    setuid(user_uid);
/*      if (debug_opt) */
/*  	fprintf(stderr, "uid : %d euid : %d\n", getuid(), geteuid()); */

    if ( ! is_allowed(user_str) ) {
	die("User \"%s\" is not allowed to use %s. Aborting.",
	    user_str, prog_name);	    
    }

    /* check for broken pipes ... */
    signal(SIGPIPE, sigpipe_handler);

    if ( cmd_str == NULL )
	return_code = interactive_mode(fd);
    else
	return_code = talk_fcron(cmd_str, fd);

    xexit( (return_code == OK ) ? EXIT_OK : EXIT_ERR );

    /* never reached */
    return EXIT_OK;

}
