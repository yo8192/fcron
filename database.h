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

 /* $Id: database.h,v 1.1 2001-04-29 22:22:50 thib Exp $ */


/* functions prototypes */
extern void test_jobs(void);
extern void wait_chld(void);
extern void wait_all(int *counter);
extern time_t time_to_sleep(time_t lim);
extern time_t check_lavg(time_t lim);
extern void set_next_exe(CL *line, char option);
extern void set_next_exe_notrun(CL *line, char context);
extern void mail_notrun(CL *line, char context, struct tm *since);
extern void insert_nextexe(CL *line);
extern void add_serial_job(CL *line);
extern void add_lavg_job(CL *line);
extern void run_serial_job(void);
