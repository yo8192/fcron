/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2006 Thibault Godouet <fcron@free.fr>
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

 /* $Id: job.h,v 1.8 2006-01-11 00:47:15 thib Exp $ */

#ifndef __JOB_H__
#define __JOB_H__

/* functions prototypes */
extern int change_user(struct cl_t *cl);
extern void run_job(struct exe_t *exeent);
extern FILE *create_mail(struct cl_t *line, char *subject);
extern void launch_mailer(struct cl_t *line, FILE *mailf);

#endif /* __JOB_H__ */
