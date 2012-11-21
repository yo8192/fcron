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

 /* $Id: log.c,v 1.18 2007-04-14 18:04:09 thib Exp $ */

/* This code is inspired by Anacron's sources of
   Itai Tzur <itzur@actcom.co.il> */


#include "fcron.h"

#include "log.h"

#include <sys/types.h>
#include <sys/socket.h>

#ifdef DEBUG
char debug_opt = 1;       /* set to 1 if we are in debug mode */
#else
char debug_opt = 0;       /* set to 1 if we are in debug mode */
#endif


static void xopenlog(void);
char* make_msg(const char *append, char *fmt, va_list args);
void log_syslog_str(int priority, char *msg);
void log_console_str(char *msg);
void log_fd_str(int fd, char *msg);
static void log_syslog(int priority, int fd, char *fmt, va_list args);
static void log_e(int priority, char *fmt, va_list args);
#ifdef HAVE_LIBPAM
static void log_pame(int priority, pam_handle_t *pamh, int pamerrno,
		     char *fmt, va_list args);
#endif

static char truncated[] = " (truncated)";
static int log_open = 0;


static void
xopenlog(void)
{
    if (!log_open) {
	openlog(prog_name, LOG_PID, SYSLOG_FACILITY);
	log_open=1;
    }
}


void
xcloselog()
{
    if (log_open) closelog();
    log_open = 0;
}


/* Construct the message string from its parts, and append a string to it */
char *
make_msg(const char *append, char *fmt, va_list args)
{
    int len;
    char *msg = NULL;

    if ( (msg = calloc(1, MAX_MSG + 1)) == NULL )
	return NULL;
    /* There's some confusion in the documentation about what vsnprintf
     * returns when the buffer overflows.  Hmmm... */
    len = vsnprintf(msg, MAX_MSG + 1, fmt, args);
    if ( append != NULL ) {
	size_t size_to_cat = ( (MAX_MSG-len) > 0) ? (MAX_MSG-len) : 0;
	strncat(msg, ": ", size_to_cat); 
	strncat(msg, append, size_to_cat);
	len += 2 + strlen(append);
    }
    if (len >= MAX_MSG)
	strcpy(msg + (MAX_MSG - 1) - sizeof(truncated), truncated);

    return msg;
}


/* log a simple string to syslog if needed */
void
log_syslog_str(int priority, char *msg)
{
    if (dosyslog) {
	xopenlog();
	syslog(priority, "%s", msg);
    }

}

/* log a simple string to console if needed */
void
log_console_str(char *msg)
{
    if (foreground == 1) {
	time_t t = time(NULL);
	struct tm *ft;
	char date[30];

	ft = localtime(&t);
	date[0] = '\0';
	strftime(date, sizeof(date), "%H:%M:%S", ft);
	fprintf(stderr, "%s %s\n", date, msg);

    }
}

/* log a simple string to fd if needed */
void
log_fd_str(int fd, char *msg)
{
    if ( fd >= 0 ) {
	send(fd, msg, strlen(msg), 0);
	send(fd, "\n", strlen("\n"), 0);
    }
}

/* Log a message, described by "fmt" and "args", with the specified 
 * "priority". */
/* write it also to fd if positive, and to stderr if foreground==1 */
static void
log_syslog(int priority, int fd, char *fmt, va_list args)
{
    char *msg;

    if ( (msg = make_msg(NULL, fmt, args)) == NULL)
	return;

    log_syslog_str(priority, msg);
    log_console_str(msg);
    log_fd_str(fd, msg);

    Free_safe(msg);
}

/* Same as log_syslog(), but also appends an error description corresponding
 * to "errno". */
static void
log_e(int priority, char *fmt, va_list args)
{
    int saved_errno;
    char *msg;

    saved_errno=errno;

    if ( (msg = make_msg(strerror(saved_errno), fmt, args)) == NULL )
	return ;

    log_syslog_str(priority, msg);
    log_console_str(msg);

    Free_safe(msg);
}


#ifdef HAVE_LIBPAM
/* Same as log_syslog(), but also appends an error description corresponding
 * to the pam_error. */
static void
log_pame(int priority, pam_handle_t *pamh, int pamerrno, char *fmt, va_list args)
{
    char *msg;

    if ( (msg = make_msg(pam_strerror(pamh, pamerrno), fmt, args)) == NULL )
        return ;

    log_syslog_str(priority, msg);
    log_console_str(msg);

    xcloselog();

    Free_safe(msg);
}
#endif


/* Log an "explain" level message */
void
explain(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_syslog(EXPLAIN_LEVEL, -1, fmt, args);
    va_end(args);
}

/* as explain(), but also write message to fd if positive */
void
explain_fd(int fd, char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_syslog(EXPLAIN_LEVEL, fd, fmt, args);
    va_end(args);
}


/* Log an "explain" level message, with an error description */
void
explain_e(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_e(EXPLAIN_LEVEL, fmt, args);
    va_end(args);
}


/* Log a "warning" level message */
void
warn(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_syslog(WARNING_LEVEL, -1, fmt, args);
    va_end(args);
}

/* as warn(), but also write message to fd if positive */
void
warn_fd(int fd, char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_syslog(WARNING_LEVEL, fd, fmt, args);
    va_end(args);
}


/* Log a "warning" level message, with an error description */
void
warn_e(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_e(WARNING_LEVEL, fmt, args);
    va_end(args);
}


/* Log a "complain" level message */
void
error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_syslog(COMPLAIN_LEVEL, -1, fmt, args);
    va_end(args);
}

/* as error(), but also write message to fd if positive */
void
error_fd(int fd, char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_syslog(COMPLAIN_LEVEL, fd, fmt, args);
    va_end(args);
}


/* Log a "complain" level message, with an error description */
void
error_e(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_e(COMPLAIN_LEVEL, fmt, args);
    va_end(args);
}


#ifdef HAVE_LIBPAM
/* Log a "complain" level message, with a PAM error description */
void
error_pame(pam_handle_t *pamh, int pamerrno, char *fmt, ...)
{
    va_list args;

    xcloselog();  /* PAM is likely to have used openlog() */

    va_start(args, fmt);
    log_pame(COMPLAIN_LEVEL, pamh, pamerrno, fmt, args);
    va_end(args);
}
#endif

/* Log a "complain" level message, and exit */
void
die(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_syslog(COMPLAIN_LEVEL, -1, fmt, args);
    va_end(args);
    if (getpid() == daemon_pid) {
        error("Aborted");
    }
    else {
        error("fcron child aborted: this does not affect the main fcron daemon,"
        " but this may prevent a job from being run or an email from being sent.");
    }

    exit(EXIT_ERR);

}  


/* Log a "complain" level message, with an error description, and exit */
void
die_e(char *fmt, ...)
{
   va_list args;
   int err_no = 0;

   err_no = errno;

   va_start(args, fmt);
   log_e(COMPLAIN_LEVEL, fmt, args);
   va_end(args);
   if (getpid() == daemon_pid) {
        error("Aborted");
    }
    else {
        error("fcron child aborted: this does not affect the main fcron daemon,"
        " but this may prevent a job from being run or an email from being sent.");
    }


   exit(err_no);

}  

#ifdef HAVE_LIBPAM
/* Log a "complain" level message, with a PAM error description, and exit */
void
die_pame(pam_handle_t *pamh, int pamerrno, char *fmt, ...)
{
    va_list args;

    xcloselog();  /* PAM is likely to have used openlog() */

    va_start(args, fmt);
    log_pame(COMPLAIN_LEVEL, pamh, pamerrno, fmt, args);
    va_end(args);
    pam_end(pamh, pamerrno);  
    if (getpid() == daemon_pid) error("Aborted");

    exit(EXIT_ERR);

}
#endif

/* Log a "debug" level message */
void
Debug(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_syslog(DEBUG_LEVEL, -1, fmt, args);
    va_end(args);
}

/* write message to fd, and to syslog in "debug" level message if debug_opt */
void
send_msg_fd_debug(int fd, char *fmt, ...)
{
    char *msg;

    va_list args;

    va_start(args, fmt);

    if ( (msg = make_msg(NULL, fmt, args)) == NULL)
	return;

    if ( debug_opt )
	log_syslog_str(DEBUG_LEVEL, msg);

    log_fd_str(fd, msg);

    Free_safe(msg);

    va_end(args);
}

/* write message to fd */
void
send_msg_fd(int fd, char *fmt, ...)
{
    char *msg;

    va_list args;

    va_start(args, fmt);

    if ( (msg = make_msg(NULL, fmt, args)) == NULL)
	return;

    log_fd_str(fd, msg);

    Free_safe(msg);

    va_end(args);
}


