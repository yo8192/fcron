
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

 /* $Id: allow.c,v 1.9 2001-06-22 21:10:30 thib Exp $ */

#include "fcrontab.h"
#include "allow.h"

int
in_file(char *str, char *file)
    /* return -1 if file doesn't exist
     *        0 if string is not in file,
     *        1 if it is in file
     *   and  2 if file contains "all" string */
{
    char buf[LINE_LEN];
    FILE *f = NULL;
    char *start = NULL;

    if ( access(file, F_OK) != 0 )
	/* file does not exist */
	return -1;

    if ( (f = fopen(file, "r")) == NULL ) {
	if (errno == ENOENT)
	    return -1;
	else
	    die_e("could not open %s", file);
    }

    while ( fgets(buf, sizeof(buf), f) != NULL ) {

	/* skip leading and trailing blanks, comments */
	start = buf;
	while ( *start != '\0' && isspace( (int) *start) )
	    start++;
	if ( *start == '#' || *start == '\0' )
	    continue;
	remove_blanks(start);

	if ( strcmp(str, start) == 0 )
	    return 1;
	if ( strcmp(start, "all") == 0 )
	    return 2;
    }

    /* if execution gets here, string is not in file */
    return 0;


}

int
is_allowed(char *user)
    /* return 1 if user is allowed to use this soft
     * otherwise return 0 */
{
    char allow = 0;
    char deny = 0;

    /* check if user is in passwd file */
    if ( ! getpwnam(user) )
	return 0;

    /* check if user is in fcron.allow and/or in fcron.deny files */
    allow = in_file(user, fcronallow);
    deny = in_file(user, fcrondeny);

    if ( allow == -1 && deny == -1 )
	/* neither fcron.allow nor fcron.deny exist :
	 * we consider that user is allowed */
	return 1;

    if ( allow == -1 && deny == 0 )
	return 1;
    
    if ( deny == -1 && allow == 1 )
	return 1;

    if ( allow == 1 )
	if ( deny != 1 )
	    return 1;
    if ( allow == 2 )
	if ( deny <= 0 )
	    return 1;

    /* if we gets here, user is not allowed */
    return 0;

}
