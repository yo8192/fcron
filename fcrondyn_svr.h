/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2016 Thibault Godouet <fcron@free.fr>
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


/* This file describe the communication protocol between fcron and fcrondyn */

#ifndef __FCRONDYN_SVR_H__
#define __FCRONDYN_SVR_H__

#include "dyncom.h"
#include "select.h"
#ifdef HAVE_SYS_RESOURCE_H
/* needed by setpriority() */
#include <sys/resource.h>
#endif

/* functions prototypes */
extern void fcrondyn_socket_init(select_instance * si);
extern void fcrondyn_socket_check(select_instance * si);
extern void fcrondyn_socket_close(select_instance * si);

/* struct used by fcron : */
typedef struct fcrondyn_cl {
    struct fcrondyn_cl *fcl_next;
    int fcl_sock_fd;
    char *fcl_user;
    time_t fcl_idle_since;
    int fcl_cmd_len;
    long int *fcl_cmd;
} fcrondyn_cl;


#endif                          /* __FCRONDYN_SVR_H__ */
