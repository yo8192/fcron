/* 
 *  gloadavg.c - get load average for Linux
 *  Copyright (C) 1993  Thomas Koenig
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "fcron.h"

#define _POSIX_SOURCE 1

/* Local headers */

#include "getloadavg.h"

/* Global functions */

int
getloadavg(double *result, int n)
/* return the current load average as a floating point number,
 * the number of load averages read, or <0 for error
 */
{
    FILE *fp;
    int i;

    if (n > 3)
	n = 3;

    if ((fp = fopen(PROC "/loadavg", "r")) == NULL) {
	error_e("could not open '"PROC"/loadavg'"
		" (make sure procfs is mounted)");
	i = -1;
    }
    else {
	for (i = 0; i < n; i++) {
	    if (fscanf(fp, "%lf", result) != 1)
		goto end;
	    result++;
	}
    }
  end:
    fclose(fp);
    return (i<0) ? i : i;
}
