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

 /* $Id: subs.h,v 1.2 2001-05-15 00:51:17 thib Exp $ */

#ifndef __SUBS_H__
#define __SUBS_H__

/* functions prototypes */
extern int remove_blanks(char *str);
extern char *strdup2(const char *);
extern int temp_file(char **name);
extern int save_type(FILE *f, short int type);
extern int save_str(FILE *f, short int type, char *str);
extern int save_strn(FILE *f, short int type, char *str, short int size);
extern int save_lint(FILE *f, short int type, long int value);

#endif /* __SUBS_H__ */
