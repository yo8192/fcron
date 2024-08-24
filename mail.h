/*
 * FCRON - periodic command scheduler
 *
 *  Copyright 2000-2021 Thibault Godouet <fcron@free.fr>
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


#ifndef __MAIL_H__
#define __MAIL_H__

#define MAIL_LINE_LEN_MAX 998 /* RFC5322's max line length */
#define FROM_HEADER_KEY "From: "
#define MAIL_FROM_VALUE_LEN_MAX (MAIL_LINE_LEN_MAX - sizeof(FROM_HEADER_KEY))

#endif                          /* __MAIL_H__ */
