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

 /* $Id: global.h,v 1.3 2000-05-24 17:51:55 thib Exp $ */

#ifndef __GLOBALH__
#define __GLOBALH__

#include <ctype.h>
#include <errno.h>
#ifdef __linux__
#include <getopt.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "bitstring.h"         

#include "config.h"



/* none configurable constants */
#define FILEVERSION "001"  /* syntax's version of fcrontabs : 
			    * must have a length of 3 characters */



/* macros */
#define arysize(ary)	(sizeof(ary)/sizeof((ary)[0]))

#define Alloc(ptr, type) \
        if( (ptr = calloc(1, sizeof(type))) == NULL ) { \
            fprintf(stderr, "Could not calloc."); \
            exit(EXIT_ERR); \
        }

#define debug if(debug_opt) Debug

typedef struct env_t {
    char         *e_name;       /* env name                             */
    char         *e_val;        /* env value                            */
    struct env_t *e_next;
} env_t ;

typedef struct CF {
    struct CF    *cf_next;
    struct CL    *cf_line_base;
    char	 *cf_user;	/* user-name			        */
    char         *cf_mailto;    /* mail output's to mail_user           */
    struct env_t *cf_env_base;  /* list of all env variables to set     */
    int		 cf_running;	/* number of jobs running               */
} CF;

/* warning : do not change the order of the members of this structure
 *   because some tests made are dependent to that order */
typedef struct CL {
    struct CL    *cl_next;
    char	 *cl_shell;	/* shell command			*/
    pid_t	 cl_pid;	/* running pid, 0, or armed (-1)        */
    pid_t	 cl_mailpid;	/* mailer pid or 0                  	*/
    int		 cl_mailfd;	/* running pid is for mail		*/
    int		 cl_mailpos;	/* 'empty file' size			*/
    /* see bitstring(3) man page for more details */
    bitstr_t	 bit_decl(cl_mins, 60);  /* 0-59			*/
    bitstr_t	 bit_decl(cl_hrs, 24);	/* 0-23				*/
    bitstr_t	 bit_decl(cl_days, 32);	/* 1-31				*/
    bitstr_t	 bit_decl(cl_mons, 12);	/* 0-11                 	*/
    bitstr_t	 bit_decl(cl_dow, 8);	/* 0-7, 0 and 7 are both Sunday	*/
    time_t       cl_timefreq;    /* Run every n min                     */
    short int    cl_runfreq;     /* Run once every n matches            */
    time_t       cl_remain;      /* remaining until next execution      */
    time_t       cl_nextexe;     /* time and date of the next execution */
} CL;



#endif /* __GLOBALH__ */

