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

 /* $Id: global.h,v 1.19 2000-09-12 19:53:10 thib Exp $ */


/* 
   WARNING : this file should not be modified.
   Compilation's options are in config.h
*/

#ifndef __GLOBALH__
#define __GLOBALH__

/* config.h must be included before every other includes */
#include "config.h"


#include <ctype.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <pwd.h>
#include <signal.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#elif HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#elif HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif

#include "bitstring.h"         
#include "option.h"


#define FILEVERSION "012"  /* syntax's version of fcrontabs : 
			    * must have a length of 3 characters */

/* you should not change this (nor need to do it) */
#define ERR     -1           
#define OK       0


/* macros */
#define Alloc(ptr, type) \
        if( (ptr = calloc(1, sizeof(type))) == NULL ) \
            die_e("Could not calloc.");

#define debug if(debug_opt) Debug

typedef struct env_t {
    char         *e_val;        /* env value                            */
    struct env_t *e_next;
} env_t ;

typedef struct CF {
    struct CF    *cf_next;
    struct CL    *cf_line_base;
    char	 *cf_user;	/* user-name			        */
    struct env_t *cf_env_base;  /* list of all env variables to set     */
    int		 cf_running;	/* number of jobs running               */
} CF;

/* warning : do not change the order of the members of this structure
 *   because some tests made are dependent to that order */
typedef struct CL {
    struct CL     *cl_next;
    struct CF     *cl_file;       /* the file in which the line is        */
    unsigned short cl_option;     /* options for that line (see option.h) */
    char	  *cl_shell;      /* shell command			  */
    char           cl_lavg[3];    /* load averages needed (1, 5, 15 mins) */
    time_t         cl_until;      /* timeout of the wait for a lavg value */
    char           cl_nice;       /* nice value to control priority       */
    uid_t          cl_runas;      /* determine permissions of the job     */
    uid_t          cl_mailto;     /* mail output to cl_mailto             */
    pid_t	   cl_pid;	  /* running pid, 0, or armed (-1)        */
    time_t         cl_nextexe;    /* time and date of the next execution  */
    short int      cl_remain;     /* remaining until next execution       */
    time_t         cl_timefreq;   /* Run every n seconds                  */
    short int      cl_runfreq;    /* Run once every n matches             */
    /* see bitstring(3) man page for more details */
    bitstr_t	   bit_decl(cl_mins, 60); /* 0-59		          */
    bitstr_t	   bit_decl(cl_hrs, 24);  /* 0-23			  */
    bitstr_t	   bit_decl(cl_days, 32); /* 1-31			  */
    bitstr_t	   bit_decl(cl_mons, 12); /* 0-11                	  */
    bitstr_t	   bit_decl(cl_dow, 8);	  /* 0-7, 0 and 7 are both Sunday */
} CL;

typedef struct job {
    struct CL    *j_line;
    struct job   *j_next;
} job;

typedef struct lavg {
    struct CL  *l_line;  
    time_t      l_until;   /* the timeout of the wait for load averages */
} lavg;

#endif /* __GLOBALH__ */

