/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2002 Thibault Godouet <fcron@free.fr>
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

 /* $Id: fcron.h,v 1.25 2001-12-23 22:04:41 thib Exp $ */

#ifndef __FCRON_H__
#define __FCRON_H__

#include "global.h"

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#elif HAVE_SYS_DIRENT_H
#include <sys/dirent.h>
#elif HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <grp.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef HAVE_GETLOADAVG
#include "getloadavg.h"
#endif

/* global variables */
extern time_t now;
extern char debug_opt;
extern char foreground;
extern char *cdir;
extern pid_t daemon_pid;
extern mode_t saved_umask;
extern char *prog_name;
extern char sig_hup;
extern struct CF *file_base;
extern struct job *queue_base;
extern struct CL **serial_array;
extern short int serial_array_size;
extern short int serial_array_index;
extern short int serial_num;
extern short int serial_running;
extern short int serial_max_running; 
extern struct exe *exe_array;
extern short int exe_array_size;
extern short int exe_num;
extern struct lavg *lavg_array;
extern short int lavg_array_size;
extern short int lavg_num;
extern short int lavg_serial_running;
/* end of global variables */


/* functions prototypes */

/* fcron.c */
extern void xexit(int exit_value);

#endif /* __FCRON_H */

