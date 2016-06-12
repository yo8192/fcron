/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2014 Thibault Godouet <fcron@free.fr>
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

#include "select.h"
#include "unistd.h"
#include "errno.h"
#include "global.h"

void
select_init(struct select_instance *si)
    /* Initialize the structure (zero it) */
{
    FD_ZERO(&si->readfds);
    FD_ZERO(&si->__readfds_master);
    si->retcode = 0;
    si->__fd_max = -1;
}

void
select_add_read(select_instance *si, int fd)
    /* Add a fd to the read set */
{
    FD_SET(fd, &si->__readfds_master);
    if (fd > si->__fd_max) {
        si->__fd_max = fd;
    }
    debug("select: added fd %d (fd_max=%d)", fd, si->__fd_max);

}

void
select_rm_read(select_instance *si, int fd)
    /* remove a fd to the read set */
{
    FD_CLR(fd, &si->__readfds_master);
    if (fd >= si->__fd_max) {
        /* fds in the fd_set may not be continuous, so we need
         * to check them all to find the new max */
        int i;
        for (i=fd; i >= 0; i--) {
            if (FD_ISSET(i, &si->__readfds_master))
            si->__fd_max = i;
        }
    }
    debug("select: removed fd %d (fd_max=%d)", fd, si->__fd_max);
}

void
select_call(struct select_instance *si, struct timeval *timeout)
    /* call select for instance 'si' */
{
    /* re-initialize readfds -- select() will change it */
    si->readfds = si->__readfds_master;

    si->retcode = select(si->__fd_max + 1, &si->readfds, NULL, NULL, timeout);
    if (si->retcode < 0 && errno != EINTR) {
        die_e("select returned %d", errno);
    }
}

