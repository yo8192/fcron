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

 /* $Id: fcrontab.h,v 1.2 2000-05-15 18:28:42 thib Exp $ */

#ifndef __FCRONTABH__
#define __FCRONTABH__

#include "global.h"


/* macros */
#define Skip_blanks(ptr) \
        while((*ptr == ' ') || (*ptr == '\t')) \
	ptr++;

/* global variables */
extern char debug_opt;
extern CF *file_base;

/* prototype definition */

/* fileconf.c */
extern int read_file(char *file_name, char *user);
extern void delete_file(const char *user_name);
extern void save_file(char *path);
/* end of fileconf.c */

/* subs.c */
extern int remove_blanks(char *str);
extern char *strdup2(const char *);
/* end of subs.c */

/* log.c */
extern void xcloselog(void);
extern char *make_msg(char *fmt, va_list args);
extern void explain(char *fmt, ...);
extern void explain_e(char *fmt, ...);
extern void warn(char *fmt, ...);
extern void warn_e(char *fmt, ...);
extern void error(char *fmt, ...);
extern void error_e(char *fmt, ...);
extern void die(char *fmt, ...);
extern void die_e(char *fmt, ...);
extern void Debug(char *fmt, ...);
/* end of log.c */

/* allow.c */
extern int is_allowed(char *user);
/* end of allow.c */


#endif /* __FCRONTABH__ */
