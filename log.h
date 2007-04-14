/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2007 Thibault Godouet <fcron@free.fr>
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

 /* $Id: log.h,v 1.9 2007-04-14 18:04:10 thib Exp $ */

#ifndef __LOG_H__
#define __LOG_H__

/* functions prototypes */
extern void xcloselog(void);
extern void explain(char *fmt, ...);
extern void explain_fd(int fd, char *fmt, ...);
extern void explain_e(char *fmt, ...);
extern void warn(char *fmt, ...);
extern void warn_fd(int fd, char *fmt, ...);
extern void warn_e(char *fmt, ...);
extern void error(char *fmt, ...);
extern void error_fd(int fd, char *fmt, ...);
extern void error_e(char *fmt, ...);
extern void die(char *fmt, ...);
extern void die_e(char *fmt, ...);
#ifdef HAVE_LIBPAM
extern void error_pame(pam_handle_t *pamh, int pamerrno, char *fmt, ...);
extern void die_pame(pam_handle_t *pamh, int pamerrno, char *fmt, ...);
#endif
extern void Debug(char *fmt, ...);
extern void send_msg_fd_debug(int fd, char *fmt, ...);
extern void send_msg_fd(int fd, char *fmt, ...);

#endif /* __LOG_H__ */
