/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2025 Thibault Godouet <fcron@free.fr>
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

/* This file contains helper functions around an instance of select() */

#ifndef __SELECT_H__
#define __SELECT_H__

#include "config.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif


typedef struct select_instance {
    /* public (read-only) */
    int retcode;                /* return value of the last call of select() */
    fd_set readfds;             /* to check the status of fds after select_call */
    /* private */
    fd_set __readfds_master;    /* select() modifies readfds, so we maintain a list in parallel */
    int __fd_max;
} select_instance;

/* functions prototypes */
extern void select_init(struct select_instance *si);
extern void select_add_read(select_instance * si, int fd);
extern void select_rm_read(select_instance * si, int fd);
extern void select_call(struct select_instance *si, struct timeval *timeout);

#endif                          /* __SELECT_H__ */
