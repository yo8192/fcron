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

 /* $Id: database.h,v 1.2 2001-05-15 00:48:58 thib Exp $ */

#ifndef __DATABASE_H__
#define __DATABASE_H__

/* functions prototypes */
extern void test_jobs(void);
extern void wait_chld(void);
extern void wait_all(int *counter);
extern time_t time_to_sleep(time_t lim);
extern time_t check_lavg(time_t lim);
extern void set_next_exe(CL *line, char option);
#define NO_GOTO 1      /* set_next_exe() : no goto_non_matching() */
#define NO_GOTO_LOG 2  /* set_next_exe() : NO_GOTO but also log nextexe time */
extern void set_next_exe_notrun(CL *line, char context);
#define LAVG 1         /* set_next_exe_notrun() : context */
#define SYSDOWN 2      /* set_next_exe_notrun() : context */
extern void mail_notrun(CL *line, char context, struct tm *since);
extern void insert_nextexe(CL *line);
extern void add_serial_job(CL *line);
extern void add_lavg_job(CL *line);
extern void run_serial_job(void);

#endif /* __DATABASE_H__ */
