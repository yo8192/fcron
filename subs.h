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

 /* $Id: subs.h,v 1.11 2007-04-14 18:04:23 thib Exp $ */

#ifndef __SUBS_H__
#define __SUBS_H__


/* global variables */

/* fcron.conf parameters */
extern char *fcronconf;
extern char *fcronallow;
extern char *fcrondeny;
extern char *fcrontabs;
extern char *pidfile;
extern char *fifofile;
extern char *editor;
extern char *shell;
extern char *sendmail;
/* end of global variables */

/* functions prototypes */
extern uid_t get_user_uid_safe(char *username);
extern gid_t get_group_gid_safe(char *groupname);
extern int remove_blanks(char *str);
extern int strcmp_until(const char *left, const char *right, char until);
extern char *strdup2(const char *);
extern void *alloc_safe(size_t len, const char * desc);
extern void *realloc_safe(void *ptr, size_t len, const char * desc);
extern void free_safe(void *ptr);
extern int get_word(char **str);
extern int temp_file(char **name);
extern void read_conf(void);
extern void free_conf(void);
extern void my_unsetenv(const char *name);
extern void my_setenv_overwrite(const char *name, const char *value);

#endif /* __SUBS_H__ */
