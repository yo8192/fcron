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

 /* $Id: pam.h,v 1.2 2001-12-23 22:04:05 thib Exp $ */

#ifndef __PAM_H__
#define __PAM_H__

#include <security/pam_appl.h>

/* global variables */
extern pam_handle_t *pamh;
extern const struct pam_conv apamconv;

#endif /* __PAM_H__ */
