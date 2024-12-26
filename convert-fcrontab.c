
/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2024 Thibault Godouet <fcron@free.fr>
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


#include "convert-fcrontab.h"
#include "mem.h"


void info(void);
void usage(void);
void convert_file(char *file_name);
char *read_str(FILE * f, char *buf, int max);
void delete_file(cf_t * file);

char *cdir = FCRONTABS;         /* the dir where are stored users' fcrontabs */

/* needed by log part : */
char *prog_name = NULL;
char foreground = 1;
pid_t daemon_pid = 0;
uid_t rootuid = 0;
gid_t rootgid = 0;

void
info(void)
    /* print some informations about this program :
     * version, license */
{
    fprintf(stderr,
            "convert-fcrontab " VERSION_QUOTED "\n"
            "Copyright " COPYRIGHT_QUOTED " Thibault Godouet <fcron@free.fr>\n"
            "This program is free software distributed WITHOUT ANY WARRANTY.\n"
            "See the GNU General Public License for more details.\n"
            "\n"
            "WARNING: this program is not supposed to be installed on the "
            "system. It is only used at installation time to convert the "
            "the binary fcrontabs in the old format (fcron < 1.1.0, which "
            "was published in 2001) to the present one.");

    exit(EXIT_OK);

}


void
usage()
  /*  print a help message about command line options and exit */
{
    fprintf(stderr, "\nconvert-fcrontab " VERSION_QUOTED "\n\n"
            "convert-fcrontab -h\n"
            "convert-fcrontab -V\n"
            "convert-fcrontab user\n"
            "  Update the fcrontab of \"user\" to fit the new binary format.\n"
            "\n"
            "WARNING: this program is not supposed to be installed on the "
            "system. It is only used at installation time to convert the "
            "the binary fcrontabs in the old format (fcron < 1.1.0, which "
            "was published in 2001) to the present one.");

    exit(EXIT_ERR);
}


char *
read_str(FILE * f, char *buf, int max)
    /* return a pointer to string read from file f
     * if it is non-zero length */
{
    int i;

    for (i = 0; i < max; i++)
        if ((buf[i] = fgetc(f)) == '\0')
            break;
    buf[max - 1] = '\0';

    if (strlen(buf) == 0)
        return NULL;
    else
        return strdup2(buf);

}


void
delete_file(cf_t * file)
    /* free a file if user_name is not null 
     *   otherwise free all files */
{
    cl_t *line = NULL;
    cl_t *cur_line = NULL;


    /* free lines */
    cur_line = file->cf_line_base;
    while ((line = cur_line) != NULL) {
        cur_line = line->cl_next;
        Free_safe(line->cl_shell);
        Free_safe(line->cl_mailfrom);
        Free_safe(line->cl_mailto);
        Free_safe(line->cl_runas);
        Free_safe(line);
    }

    /* free env variables */
    env_list_destroy(file->cf_env_list);

    /* finally free file itself */
    Free_safe(file->cf_user);
    Free_safe(file);

}


void
convert_file(char *file_name)
/* this functions is a mix of read_file() from version 1.0.3 and save_file(),
 * so you can read more comments there */
{
    cf_t *file = NULL;
    cl_t *line = NULL;
    char *env = NULL;
    FILE *f = NULL;
    char buf[LINE_LEN];
    time_t t_save = 0;
    struct stat file_stat;

    explain("Converting %s's fcrontab ...", file_name);

    Alloc(file, cf_t);
    /* open file */
    if ((f = fopen(file_name, "r")) == NULL)
        die_e("Could not read %s", file_name);

    if (fstat(fileno(f), &file_stat) != 0)
        die_e("Could not stat %s", file_name);

    bzero(buf, sizeof(buf));

    if (fgets(buf, sizeof(buf), f) == NULL ||
        strncmp(buf, "fcrontab-017\n", sizeof("fcrontab-017\n")) != 0) {

        error("File %s is not valid: ignored.", file_name);
        error("Maybe this file has been generated by a too old version "
              "of fcron ( <= 0.9.4), or is already in the new binary format.");
        error("In this case, you should reinstall it using fcrontab"
              " (but be aware that you may lose some data as the last "
              "execution time and date as if you run a fcrontab -z -n).");
        exit(EXIT_ERR);
    }

    if ((file->cf_user = read_str(f, buf, sizeof(buf))) == NULL)
        die_e("Cannot read user's name");

    if (fscanf(f, "%" ATTR_SIZE_TIMET "d", CAST_TIMET_PTR & t_save) != 1)
        error("could not get time and date of saving");

    /* read env variables */
    while ((env = read_str(f, buf, sizeof(buf))) != NULL) {
        env_list_putenv(file->cf_env_list, env, 1);
        Free_safe(env);
    }

    /* read lines */
    Alloc(line, cl_t);
    while (fread(line, sizeof(cl_t), 1, f) == 1) {

        if ((line->cl_shell = read_str(f, buf, sizeof(buf))) == NULL) {
            error("Line is not valid (empty shell command) : ignored");
            continue;
        }
        if ((line->cl_runas = read_str(f, buf, sizeof(buf))) == NULL) {
            error("Line is not valid (empty runas field) : ignored");
            continue;
        }
        if ((line->cl_mailto = read_str(f, buf, sizeof(buf))) == NULL) {
            error("Line is not valid (empty mailto field) : ignored");
            continue;
        }

        line->cl_next = file->cf_line_base;
        file->cf_line_base = line;
        Alloc(line, cl_t);

    }

    Free_safe(line);

    xfclose(&f);

    /* open a temp file in write mode and truncate it */
    strcpy(buf, "tmp_");
    strncat(buf, file_name, sizeof(buf) - sizeof("tmp_") - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* everything's ok : we can override the src file safely */
    if (rename(buf, file_name) != 0)
        error_e("Could not rename %s to %s", buf, file_name);

    save_file_safe(file, file_name, "convert-fcrontab", file_stat.st_uid,
                   file_stat.st_gid, t_save);

    delete_file(file);
}

int
main(int argc, char *argv[])
{
    int c;
    extern char *optarg;
    extern int optind, opterr, optopt;
    char *user_to_update = NULL;

    rootuid = get_user_uid_safe(ROOTNAME);
    rootgid = get_group_gid_safe(ROOTGROUP);

    if (strrchr(argv[0], '/') == NULL)
        prog_name = argv[0];
    else
        prog_name = strrchr(argv[0], '/') + 1;


    /* constants and variables defined by command line */

    while (1) {
        c = getopt(argc, argv, "chV");
        if (c == EOF)
            break;
        switch (c) {

        case 'V':
            info();
            break;

        case 'h':
            usage();
            break;

        case 'c':
            Set(fcronconf, optarg);
            break;

        case ':':
            fprintf(stderr, "(setopt) Missing parameter");
            usage();

        case '?':
            usage();

        default:
            fprintf(stderr, "(setopt) Warning: getopt returned %c", c);
        }
    }

    if (optind >= argc || argc != 2)
        usage();

    /* parse fcron.conf */
    read_conf();

    user_to_update = strdup2(argv[optind]);

    if (chdir(cdir) != 0)
        die_e("Could not change dir to " FCRONTABS);

    convert_file(user_to_update);

    exit(EXIT_OK);

}
