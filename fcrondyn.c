
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

 /* $Id: fcrondyn.c,v 1.1 2002-02-25 18:38:42 thib Exp $ */

/* fcrondyn : interact dynamically with running fcron process :
 *     - list jobs, with their status, next time of execution, etc
 *     - run, delay a job
 *     - send a signal to a job
 *     - etc ...
 */

#include "fcrondyn.h"
#include "allow.h"

char rcs_info[] = "$Id: fcrondyn.c,v 1.1 2002-02-25 18:38:42 thib Exp $";

void info(void);
void usage(void);

#ifdef DEBUG
char debug_opt = 1;       /* set to 1 if we are in debug mode */
#else
char debug_opt = 0;       /* set to 1 if we are in debug mode */
#endif

char *user = NULL;
uid_t uid = 0;
uid_t asuid = 0;

/* needed by log part : */
char *prog_name = NULL;
char foreground = 1;
char dosyslog = 1;
pid_t daemon_pid = 0;


void
info(void)
    /* print some informations about this program :
     * version, license */
{
    fprintf(stderr,
	    "fcrondyn " VERSION_QUOTED " - interact dynamically with daemon fcron\n"
	    "Copyright 2000-2002 Thibault Godouet <fcron@free.fr>\n"
	    "This program is free software distributed WITHOUT ANY WARRANTY.\n"
            "See the GNU General Public License for more details.\n"
	);

    exit(EXIT_OK);

}


void
usage(void)
  /*  print a help message about command line options and exit */
{
    fprintf(stderr, 
	    "fcrondyn [-n] file [user|-u user]\n"
	    "fcrondyn { -l | -r | -e | -z } [-n] [user|-u user]\n"
	    "fcrondyn -h\n"
	    "  -u user    specify user name.\n"
	    "  -l         list user's current fcrontab.\n"
	    "  -r         remove user's current fcrontab.\n"
	    "  -c f       make fcrontab use config file f.\n"
	    "  -d         set up debug mode.\n"
	    "  -h         display this help message.\n"
	    "\n"
	);
    
    exit(EXIT_ERR);
}

int
main(int argc, char **argv)
{

}
