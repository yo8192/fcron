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


/* Private declarations for fcrondyn_svr.c */

#ifndef __FCRONDYN_SVR_PRIV_H__
#define __FCRONDYN_SVR_PRIV_H__

/* which bit corresponds to which field ? */
#define FIELD_USER 0
#define FIELD_RQ 1
#define FIELD_PID 2
#define FIELD_SCHEDULE 3
#define FIELD_LAVG 4
#define FIELD_INDEX 5
#define FIELD_OPTIONS 6

/* the number of char we need (8 bits (i.e. fields) per char) */
#define FIELD_NUM_SIZE 1

#endif                          /* __FCRONDYN_SVR_PRIV_H__ */
