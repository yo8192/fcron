
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

 /* $Id: conf.c,v 1.55 2002-08-10 20:40:09 thib Exp $ */

#include "fcron.h"

#include "conf.h"
#include "database.h"

int read_file(const char *file_name, CF *cf);
int add_line_to_file(CL *cl, CF *cf, uid_t runas, char *runas_str,
		     time_t t_save);
int read_strn(int fd, char **str, short int size);
int read_type(int fd, short int *type, short int *size);
void synchronize_file(char *file_name);


/* this is used to create a list of files to remove, to add */
typedef struct list_t {
    char *str;
    struct list_t *next;
} list_t;


void
reload_all(const char *dir_name)
    /* save all current configuration, remove it from the memory,
     * and reload from dir_name */
{
    CF *f = NULL;

    explain("Removing current configuration from memory");

    f = file_base;
    while ( f != NULL ) {
	if ( f->cf_running > 0 )
	    wait_all( &f->cf_running );
	save_file(f);
	delete_file(f->cf_user);    

	/* delete_file remove the f file from the list :
	 * next file to remove is now pointed by file_base. */
	f = file_base;
    }
    
    synchronize_dir(dir_name);

}


void
synchronize_dir(const char *dir_name)
    /* read dir_name and create three list of files to remove,
     * new files and normal files. Then remove each file
     * listed in the first list, then read normal files,
     * finally add new files. */
{

    list_t *rm_list = NULL;
    list_t *new_list = NULL;
    list_t *file_list = NULL;    
    list_t *list_cur = NULL;
    DIR *dir;
    struct dirent *den;

    if ( strcmp(dir_name, ".") == 0 )
	explain("updating configuration from %s", fcrontabs);
    else
	explain("updating configuration from %s", dir_name);

    if ((dir = opendir("."))) {
	while ((den = readdir(dir))) {

	    if (strncmp(den->d_name, "rm.", 3) == 0) {
		/* this is a file to remove from database */
		Alloc(list_cur, list_t);
		list_cur->str = strdup2(den->d_name);
		list_cur->next = rm_list;
		rm_list = list_cur;
	    } else

		if (strncmp(den->d_name, "new.", 4) == 0) {
		    /* this is a file to append to database */
		    Alloc(list_cur, list_t);
		    list_cur->str = strdup2(den->d_name);
		    list_cur->next = new_list;
		    new_list = list_cur;
		} else 

		    if (strchr(den->d_name, '.') != NULL)
			continue;
		    else 
			/* this is a normal file : if file_base is not null,
			 * so if a database has already been created, we
			 * ignore it */
			if ( file_base == NULL ) {
			    Alloc(list_cur, list_t);
			    list_cur->str = strdup2(den->d_name);
			    list_cur->next = file_list;
			    file_list = list_cur;
			}
	    
	}
	closedir(dir);
    } else
	die("Unable to open current dir!");
            

    /* proceed to adds or removes */

    /* begin by removing files which are no longer wanted */
    for (list_cur = rm_list; list_cur; list_cur = list_cur->next ) {
	explain("removing file %s", list_cur->str + 3);
	delete_file(list_cur->str + 3);  /* len("rm.") = 3 */
	if ( remove(list_cur->str + 3) != 0 && errno != ENOENT )
	    error_e("Could not remove %s", list_cur->str + 3);
	if ( remove(list_cur->str) != 0 && errno != ENOENT )
	    error_e("Could not remove %s", list_cur->str);
    }
    
    /* then add normal files, if any, to database */
    for (list_cur = file_list; list_cur; list_cur = list_cur->next ) {
	if ( getpwnam(list_cur->str) ) {
	    explain("adding file %s", list_cur->str);	    
	    synchronize_file(list_cur->str);
	}
	else
	    error("ignoring file \"%s\" : not in passwd file.", list_cur->str);
    }

    /* finally add new files */
    for (list_cur = new_list; list_cur; list_cur = list_cur->next ) {
	if ( getpwnam(list_cur->str + 4) ) { /* len("new.") = 4 */
	    explain("adding new file %s", list_cur->str + 4);
	    synchronize_file(list_cur->str);  
	}
	else
	    error("ignoring file %s : not in passwd file.", 
		 (list_cur->str + 4));
    }

    /* free lists */
    {
	list_t *l = NULL;
	list_t *next = NULL;
	
	next = rm_list;
	while( (l = next) != NULL ) {
	    next = l->next;
	    free(l->str);
	    free(l);
	}
	    
	next = new_list;
	while( (l = next) != NULL ) {
	    next = l->next;
	    free(l->str);
	    free(l);
	}

	next = file_list;
	while( (l = next) != NULL ) {
	    next = l->next;
	    free(l->str);
	    free(l);
	}

    }

}


void
synchronize_file(char *file_name)
{

    CF *cur_f = NULL;
    char *user = NULL;

    if (strchr(file_name, '.') != NULL ) {
	/* this is a new file : we have to check if there is an old
	 * version in database in order to keep a maximum of fields
	 * (cl_nextexe) to their current value */

	CF *prev = NULL;

	/* set user name  */
	/* we add 4 to file_name pointer because of the "new."
	 * string at the beginning of a new file */	    
	user = (file_name + 4);

	for (cur_f = file_base; cur_f; cur_f = cur_f->cf_next) {
	    if ( strcmp(user, cur_f->cf_user) == 0 )
		break;
	    prev = cur_f;
	}

	if (cur_f != NULL) {
	    /* an old version of this file exist in database */

	    CF *old = NULL;
	    CL *old_l = NULL;
	    CL *new_l = NULL;
	    /* size used when comparing two line : 
	     * it's the size of all time table (mins, days ...) */
	    const size_t size=( bitstr_size(60) + bitstr_size(24) + 
				bitstr_size(32) + bitstr_size(12) +
				bitstr_size(7) );
	    
	    old = cur_f;

	    /* load new file */
	    Alloc(cur_f, CF);
	    if ( read_file(file_name, cur_f) != 0 ) {
		/* an error as occured */
		return;
	    }

	    /* assign old pointer to the old file, and move it to the first
	     * place of the list : delete_file() only remove the first
	     * occurrence of the file which has the name given in argument */
	    if (prev != NULL) {
		prev->cf_next = old->cf_next;
		old->cf_next = file_base;
		file_base = old;
	    }
	    else 
		/* this is the first file in the list : no need to move it */
		;

	    /* compare each lines between the new and the old 
	     * version of the file */
	    for (new_l = cur_f->cf_line_base; new_l; new_l = new_l->cl_next)
		for(old_l = old->cf_line_base; old_l; old_l = old_l->cl_next) {

		    /* compare the shell command and the fields 
		       from cl_mins down to cl_runfreq or the timefreq */
		    if ( strcmp(new_l->cl_shell, old_l->cl_shell) == 0 && (
			( is_freq(new_l->cl_option) && 
			  new_l->cl_timefreq == old_l->cl_timefreq ) ||
			( is_td(new_l->cl_option) &&
			  memcmp( &(new_l->cl_mins), &(old_l->cl_mins),
				  size)==0 &&
			  is_dayor(new_l->cl_option) == 
			  is_dayor(old_l->cl_option) )
			) ) {
			
			if ( new_l->cl_runfreq == old_l->cl_runfreq ) 
			    new_l->cl_remain = old_l->cl_remain;
			/* check if there is a change about the tz diff */
			if ( (new_l->cl_file->cf_tzdiff != 
			      old_l->cl_file->cf_tzdiff) &&
			     (old_l->cl_nextexe - old_l->cl_file->cf_tzdiff
			      + new_l->cl_file->cf_tzdiff > now) )
			    new_l->cl_nextexe = old_l->cl_nextexe
				- old_l->cl_file->cf_tzdiff
				+ new_l->cl_file->cf_tzdiff;
			else
			    new_l->cl_nextexe = old_l->cl_nextexe;
			insert_nextexe(new_l);

			if (debug_opt) {
			    struct tm *ftime;
			    ftime = localtime(&new_l->cl_nextexe);
			    debug("  from last conf: %s next exec %d/%d/%d"
				  " wday:%d %02d:%02d", new_l->cl_shell,
				  (ftime->tm_mon + 1), ftime->tm_mday,
				  (ftime->tm_year + 1900), ftime->tm_wday,
				  ftime->tm_hour, ftime->tm_min);
			} 
			
			break;

		    } 
		}

	    /* remove old file from the list */
	    delete_file(user);
	    
	    /* insert new file in the list */
	    cur_f->cf_next = file_base;
	    file_base = cur_f;

	    /* save final file */
	    save_file(cur_f);

	    /* delete new.user file */
	    if ( remove(file_name) != 0 )
		error_e("could not remove %s", file_name);

	}

	else {
	    /* no old version exist in database : load this file
	     * as a normal file, but change its name */
	
	    Alloc(cur_f, CF);

	    if ( read_file(file_name, cur_f) != 0 ) {
		/* an error as occured */
		return;
	    }

	    /* insert the file in the list */
	    cur_f->cf_next = file_base;
	    file_base = cur_f;

	    /* save as a normal file */
	    save_file(cur_f);

	    /* delete new.user file */
	    if ( remove(file_name) != 0 )
		error_e("could not remove %s", file_name);
	}

    }

    else {
	/* this is a normal file */
	
	Alloc(cur_f, CF);

	if ( read_file(file_name, cur_f) != 0 ) {
	    /* an error as occured */
	    return;
	}

	/* insert the file in the list */
	cur_f->cf_next = file_base;
	file_base = cur_f;

    }

}


int
read_strn(int fd, char **str, short int size)
/* read a "size"-length string in a binary fcrontab file */
{
    if ( (*str = calloc(size + 1, sizeof(char))) == NULL )
	goto err;
    
    if ( read(fd, *str, size) < size )
	goto err;
    (*str)[size] = '\0';
    return OK;

  err:
    if (*str)
	free(*str);
    *str = NULL;
    return ERR;
    
}

int
read_type(int fd, short int *type, short int *size)
/* read the type and size of the next field in a binary fcrontab file */
{
    if ( read(fd, type, sizeof(short int)) < sizeof(short int) )
	goto err;
    if ( read(fd, size, sizeof(short int)) < sizeof(short int) )
	goto err;
    
    return OK;

  err:
    return ERR;

}


/* macros for read_file() */
/* read "size" bytes from file "ff", put them in "to", and check for errors */
#define Read(to, size, err_str) \
        { \
          if ( read(fileno(ff), &(to), size) < size ) { \
            error_e(err_str); \
	    goto err; \
	  } \
        }

#define Read_strn(to, size, err_str) \
        { \
          if ( read_strn(fileno(ff), &(to), size) != OK ) { \
            error_e(err_str); \
	    goto err; \
	  } \
        }

int
read_file(const char *file_name, CF *cf)
    /* read a formated fcrontab.
       return ERR on error, OK otherwise */
{
    FILE *ff = NULL;
    CL *cl = NULL;
    env_t *env = NULL;
    char buf[LINE_LEN];
    long int bufi = 0;
    time_t t_save = 0;
    uid_t runas = 0;
    char *runas_str = NULL;
    struct stat file_stat;
    struct passwd *pass = NULL;
    short int type = 0, size = 0;

    /* open file */
    if ( (ff = fopen(file_name, "r")) == NULL ) {
	warn_e("Could not read %s (may have just been removed)", file_name);
	goto err;
    }

    /* check if this file is owned by root : otherwise, all runas fields
     * of this field should be set to the owner */
    if ( fstat(fileno(ff), &file_stat) != 0 ) {
	error_e("Could not stat %s", file_name);
	goto err;
    }

    if ( strncmp(file_name,"new.", 4) == 0 ) {
	if ( file_stat.st_uid == ROOTUID )
	    /* file is owned by root : no test needed : set runas to ROOTUID */
	    runas = ROOTUID;
	else {
	    /* this is a standard user's new fcrontab : set the runas field to
	     * the owner of the file */
	    runas = file_stat.st_uid;
	    if ( (pass = getpwuid(file_stat.st_uid)) == NULL ) {
		error_e("Could not getpwuid(%d)", file_stat.st_uid);
		goto err;
	    }
	    runas_str = strdup2(pass->pw_name);
	}
    }
    else {
	if ( file_stat.st_uid == ROOTUID )
	    /* file is owned by root : either this file has already been parsed
	     * at least once by fcron, or it is root's fcrontab */
	    runas = ROOTUID;
	else {
	    error("Non-new file %s owned by someone else than root",file_name);
	    goto err;
	}
    }

    debug("User %s Entry", file_name);
    bzero(buf, sizeof(buf));

    /* get version of fcrontab file: it permit to daemon not to load
     * a file which he won't understand the syntax, for exemple
     * a file using a depreciated format generated by an old fcrontab,
     * if the syntax has changed */
    if ( read_type(fileno(ff), &type, &size) != OK || type != S_HEADER_T ||
	 read(fileno(ff), &bufi, size) < size || bufi != S_FILEVERSION ) {
	error("File %s is not valid: ignored.", file_name);
	error("This file may have been generated by an old version of fcron.");
	error("In that case, you should try to use the converter given in the "
	      "source package, or install it again using fcrontab.");
	goto err;
    }    

    if ( read_type(fileno(ff), &type, &size) != OK || type != S_USER_T ) {
	error("Invalid binary fcrontab (no USER field)");
	goto err;
    }
    /* get the owner's name */
    if ( read_strn(fileno(ff), &cf->cf_user, size) != OK ) {
	error("Cannot read user's name : file ignored");
	goto err;
    }
    if ( runas != ROOTUID ) {
	/* we use file owner's name for more security (see above) */
	/* free the value obtained by read_strn() (we need to read it anyway
	 * to set the file ptr to the next thing to read) */
	free(cf->cf_user);
	cf->cf_user = runas_str;
    } 

    /* get the time & date of the saving */
    /* a new file generated by fcrontab has t_save set to 0 */
    if ( read_type(fileno(ff), &type, &size) != OK || type != S_TIMEDATE_T 
	 || read(fileno(ff), &t_save, size) < size ) {
	error("could not get time and date of saving");
	goto err;
    }
	
    Alloc(cl, CL);
    /* main loop : read env variables, and lines */
    while ( read_type(fileno(ff), &type, &size) == OK ) {
	/* action is determined by the type of the field */
	switch ( type ) {

	case S_ENVVAR_T:
	    /* read a env variable and add it to the env var list */
	    Alloc(env, env_t);
	    Read_strn(env->e_val, size, "Error while reading env var");
	    /* Read_strn go to "err" on error */
	    debug("  Env: \"%s\"", env->e_val );
	    env->e_next = cf->cf_env_base;
	    cf->cf_env_base = env;	    
	    break;

	case S_TZDIFF_T:
	    /* time diff between local (real) and system hour */
	    Read(bufi, size, "Error while reading tzdiff field");
	    cf->cf_tzdiff = (signed char) bufi;
	    break;

	case S_SHELL_T:
	    Read_strn(cl->cl_shell, size, "Error while reading shell field");
	    break;

	case S_RUNAS_T:
	    Read_strn(cl->cl_runas, size, "Error while reading runas field");
	    break;

	case S_MAILTO_T:
	    Read_strn(cl->cl_mailto, size, "Error while reading mailto field");
	    break;

	case S_NEXTEXE_T:
	    Read(bufi, size, "Error while reading nextexe field");
	    cl->cl_nextexe = (time_t) bufi;
	    break;

	case S_FIRST_T:
	    Read(bufi, size, "Error while reading first field");
	    cl->cl_first = (time_t) bufi;
	    break;

	case S_OPTION_T:
	    if (size < OPTION_SIZE)
		/* set the options not defined in the savefile to default */
		set_default_opt(cl->cl_option);
	    Read(cl->cl_option, size, "Error while reading option field");
	    break;

	case S_NUMEXE_T:
	    Read(cl->cl_numexe, size, "Error while reading numexe field");
	    break;

	case S_LAVG_T:
	    Read(cl->cl_lavg, size, "Error while reading lavg field");
	    break;

	case S_UNTIL_T:
	    Read(bufi, size, "Error while reading until field");
	    cl->cl_until = (time_t) bufi;
	    break;

	case S_NICE_T:
	    Read(cl->cl_nice, size, "Error while reading nice field");
	    break;

	case S_RUNFREQ_T:
	    Read(bufi, size, "Error while reading runfreq field");
	    cl->cl_runfreq = (unsigned short) bufi;
	    break;

	case S_REMAIN_T:
	    Read(bufi, size, "Error while reading remain field");
	    cl->cl_remain = (unsigned short) bufi;
	    break;

	case S_TIMEFREQ_T:
	    Read(bufi, size, "Error while reading timefreq field");
	    cl->cl_timefreq = (time_t) bufi;
	    break;

	case S_MINS_T:
	    Read(cl->cl_mins, size, "Error while reading mins field");
	    break;

	case S_HRS_T:
	    Read(cl->cl_hrs, size, "Error while reading hrs field");
	    break;

	case S_DAYS_T:
	    Read(cl->cl_days, size, "Error while reading days field");
	    break;

	case S_MONS_T:
	    Read(cl->cl_mons, size, "Error while reading mons field");
	    break;

	case S_DOW_T:
	    Read(cl->cl_dow, size, "Error while reading dow field");
	    break;

	case S_ENDLINE_T:
	    if (add_line_to_file(cl, cf, runas, runas_str, t_save) == 0)
		Alloc(cl, CL);
	    break;

	    /* default case in "switch(type)" */
	default:
	    error("Error while loading %s : unknown field type %d (ignored)",
		  file_name, type);
	    /* skip the data corresponding to the unknown field */
	    {
		/* we avoid using fseek(), as it seems not to work correctly
		 * on some systems when we use read() on the FILE stream */
		int i;
		for (i = 0; i < size; i++)
		    if ( getc(ff) == EOF )
			goto err;
	    }
	}
    }

    /* free last cl Alloc : unused */
    free(cl);

    /* check for an error */
    if ( ferror(ff) != 0 )
	error("file %s is truncated : you should reinstall it with fcrontab",
	      file_name);

    fclose(ff);

    return OK;

  err:
    if ( ff != NULL)
	fclose(ff);

    if ( cl != NULL && cl->cl_next == NULL ) {
	/* line is not yet in the line list of the file : free it */
	if ( cl->cl_shell ) free(cl->cl_shell);
	if ( cl->cl_runas) free(cl->cl_runas);
	if ( cl->cl_mailto) free(cl->cl_mailto);
	free(cl);
    }

    /* check if we have started to read the lines and env var */
    if ( cl != NULL ) { 
	/* insert the line in the file list in order to be able to free
	 * the memory using delete_file() */
    
	cf->cf_next = file_base;
	file_base = cf;

	delete_file(cf->cf_user);
	
    }
    else if (cf->cf_user != NULL)
	free(cf->cf_user);

    return ERR;

}


int
add_line_to_file(CL *cl, CF *cf, uid_t runas, char *runas_str, time_t t_save)
    /* check if the line is valid, and if yes, add it to the file cf */
{
    time_t slept = now - t_save;

    if ( cl->cl_shell == NULL || cl->cl_runas == NULL ||
	 cl->cl_mailto == NULL ) {
	error("Line is not valid (empty shell, runas or mailto field)"
	      " : ignored");
	bzero(cl, sizeof(cl));
	if (cl->cl_shell) free(cl->cl_shell);
	if (cl->cl_runas) free(cl->cl_runas);
	if (cl->cl_mailto) free(cl->cl_mailto);
	return 1;
    }

    /* set runas field if necessary (to improve security) */
    if (runas != ROOTUID) {
	if (strcmp(cl->cl_runas, runas_str) != 0)
	    warn("warning: runas(%s) is not owner (%s): overridden.",
		 cl->cl_runas, runas_str);
	Set(cl->cl_runas, runas_str);
    }
	    
    /* we need that here because the user's name contained in the
     * struct CF may be required */
    cl->cl_file = cf;


    /* check if the job hasn't been stopped during execution and insert
     * it in lavg or serial queue if it was in one at fcron's stops  */
    if (cl->cl_numexe > 0) {
	cl->cl_numexe = 0;
	if ( is_lavg(cl->cl_option) ) {
	    if ( ! is_strict(cl->cl_option) )
		add_lavg_job(cl);
	}
	else if ( is_serial(cl->cl_option) 
		  || is_serial_once(cl->cl_option) )
	    add_serial_job(cl);
	else {
	    /* job has been stopped during execution :
	     * launch it again */
	    warn("job %s did not finish : running it again.",
		 cl->cl_shell);
	    set_serial_once(cl->cl_option);
	    add_serial_job(cl);
	}
    }

    if ( is_td(cl->cl_option) ) {
    
	/* set the time and date of the next execution  */
	if ( cl->cl_nextexe <= now ) {
	    if ( cl->cl_nextexe == 0 )
		/* the is a line from a new file */
		set_next_exe(cl, NO_GOTO);		    
	    else if (cl->cl_runfreq == 1 &&
		     is_notice_notrun(cl->cl_option))
		set_next_exe_notrun(cl, SYSDOWN);
	    else if ( is_bootrun(cl->cl_option) && t_save != 0 
		      && cl->cl_runfreq != 1) {
		if ( cl->cl_remain > 0 && --cl->cl_remain > 0 ) {
		    debug("    cl_remain: %d", cl->cl_remain);
		}
		else {
		    /* run bootrun jobs */
		    cl->cl_remain = cl->cl_runfreq;
		    debug("   boot-run %s", cl->cl_shell);
		    if ( ! is_lavg(cl->cl_option) ) {
			set_serial_once(cl->cl_option);
			add_serial_job(cl);
		    }
		    else
			add_lavg_job(cl);			    
		}
		set_next_exe(cl, STD);
	    }
	    else {
		if ( is_notice_notrun(cl->cl_option) ) {
		    /* set next exe and mail user */
		    struct tm *since2 = localtime(&cl->cl_nextexe);
		    struct tm since;
		    memcpy(&since, since2, sizeof(since));
		    set_next_exe(cl, NO_GOTO);
		    mail_notrun(cl, SYSDOWN, &since);
		} 
		else
		    set_next_exe(cl, NO_GOTO);
	    }
	}
	else
	    /* value of nextexe is valid : just insert line in queue */
	    insert_nextexe(cl);
    } else {  /* is_td(cl->cl_option) */
	/* standard @-lines */
	if ( is_volatile(cl->cl_option) || cl->cl_first > 0 ) {
	    /* cl_first is always saved for a volatile line */
 	    cl->cl_nextexe = now + cl->cl_first;
 	} else
 	    cl->cl_nextexe += slept;
 
	if ( cl->cl_timefreq < 10 ) {
	    error("Invalid timefreq for %s: set to 1 day",
		  cl->cl_shell);
	    cl->cl_timefreq = 3600*24;
	}
	insert_nextexe(cl);
    }	    

    if (debug_opt) {
	struct tm *ftime;
	ftime = localtime( &(cl->cl_nextexe) );
	debug("  cmd %s next exec %d/%d/%d wday:%d %02d:%02d:%02d",
	      cl->cl_shell, (ftime->tm_mon + 1), ftime->tm_mday,
	      (ftime->tm_year + 1900), ftime->tm_wday,
	      ftime->tm_hour, ftime->tm_min, ftime->tm_sec); 
    } 

    /* add the current line to the list, and allocate a new line */
    if ( (cl->cl_id = next_id++) >= ULONG_MAX - 1)
	next_id = 0;
    cl->cl_next = cf->cf_line_base;
    cf->cf_line_base = cl;
    return 0;
}

void
delete_file(const char *user_name)
    /* free a file if user_name is not null 
     *   otherwise free all files */
{
    CF *file;
    CF *prev_file = NULL;
    CL *line;
    CL *cur_line;
    env_t *env = NULL;
    env_t *cur_env = NULL;
    struct job *j = NULL;
    struct job *prev_j;
    int i, k;
    struct CL **s_a = NULL;

    file = file_base;
    while ( file != NULL) {
	if (strcmp(user_name, file->cf_user) != 0) {
	    prev_file = file;
	    file = file->cf_next;
	    continue;
	}

	for ( i = 0; i < exe_num; i++)
	    if ( exe_array[i].e_line != NULL && 
		 exe_array[i].e_line->cl_file == file ) {
		/* we set the e_line to NULL, as so we know in wait_chld()
		 * and wait_all() the corresponding file has been removed.
		 * Plus, we decrement serial_running and lavg_serial_running
		 * as we won't be able to do it at the end of the job */
		if ( ( is_serial(exe_array[i].e_line->cl_option) ||
		       is_serial_once(exe_array[i].e_line->cl_option) ) &&
		     ! is_lavg(exe_array[i].e_line->cl_option) )
		    serial_running--;
		else if ( is_serial(exe_array[i].e_line->cl_option) &&
			  is_lavg(exe_array[i].e_line->cl_option) )
		    lavg_serial_running--;
		exe_array[i].e_line = NULL;
	    }

	/* free lavg queue entries */
	i = 0;
	while (i < lavg_num)
	    if ( lavg_array[i].l_line->cl_file == file ) {
		debug("removing %s from lavg queue",
		      lavg_array[i].l_line->cl_shell);
		lavg_array[i].l_line->cl_numexe--;
		if (i < --lavg_num) {
		    lavg_array[i] = lavg_array[lavg_num];
		    lavg_array[lavg_num].l_line = NULL;
		}
		else
		    lavg_array[i].l_line = NULL;
	    } 
	    else
		i++;

	/* free serial queue entries */
	for ( i = 0; i < serial_array_size; i++)
	    if (serial_array[i] != NULL && serial_array[i]->cl_file == file ) {
		if ( ! s_a )
		    s_a = calloc(serial_array_size, sizeof(CL *));
		debug("removing %s from serial queue",
		      serial_array[i]->cl_shell);
		serial_num--;
		serial_array[i]->cl_numexe--;
		serial_array[i] = NULL;
	    }
	/* remove from queue and move the rest of the jobs to get
	 * a queue in order without empty entries */
	if ( ! s_a )
	    goto end_of_serial_recomputing;

	if ( (k = serial_array_index + serial_num) >= serial_array_size )
	    k -= serial_array_size;
	for ( i = k = 0; i < serial_array_size; i++) {
	    if ( serial_array_index + i < serial_array_size ) {
		if ( (s_a[k] = serial_array[serial_array_index + i]) != NULL)
		    k++;
	    }
	    else
		if( (s_a[k] = 
		     serial_array[serial_array_index + i - serial_array_size])
		    != NULL)
		    k++;
	}
	free(serial_array);
	serial_array = s_a;
	serial_array_index = 0;

      end_of_serial_recomputing:
	
	/* free lines */
	cur_line = file->cf_line_base;
	while ( (line = cur_line) != NULL) {
	    cur_line = line->cl_next;

	    /* remove from the main queue */
	    prev_j = NULL;
	    for ( j = queue_base; j != NULL; j = j->j_next )
		if ( j->j_line == line ) {
		    if (prev_j != NULL) 
			prev_j->j_next = j->j_next;
		    else
			queue_base = j->j_next;
		    free(j);
		    break;
		}
		else
		    prev_j = j;

	    /* free line itself */
	    free(line->cl_shell);
	    free(line->cl_runas);
	    free(line->cl_mailto);
	    free(line);
	}
	/* delete_file() MUST remove only the first occurrence :
	 * this is needed by synchronize_file() */
	break ;
    }

    if (file == NULL)
	/* file not in the file list */
	return;
    
    /* remove file from file list */
    if (prev_file == NULL)
	file_base = file->cf_next;
    else
	prev_file->cf_next = file->cf_next;

    /* free env variables */
    cur_env = file->cf_env_base;
    while  ( (env = cur_env) != NULL ) {
	cur_env = env->e_next;
	free(env->e_val);
	free(env);
    }

    /* finally free file itself */
    free(file->cf_user);
    free(file);

}


/* save_file() error management */
#define Save_type(f, type) \
        { \
          if ( save_type(f, type) != OK ) { \
            error_e("Could not write type : file has not been saved."); \
            fclose(f); \
            remove(file->cf_user); \
            goto next_file; \
	  } \
        }

#define Save_str(f, type, str) \
        { \
          if ( save_str(f, type, str) != OK ) { \
            error_e("Could not write str : file has not been saved."); \
            fclose(f); \
            remove(file->cf_user); \
            goto next_file; \
	  } \
        }

#define Save_strn(f, type, str, size) \
        { \
          if ( save_strn(f, type, str, size) != OK ) { \
            error_e("Could not write strn : file has not been saved."); \
            fclose(f); \
            remove(file->cf_user); \
            goto next_file; \
	  } \
        }

#define Save_lint(f, type, value) \
        { \
          if ( save_lint(f, type, value) != OK ) { \
            error_e("Could not write lint : file has not been saved."); \
            fclose(f); \
            remove(file->cf_user); \
            goto next_file; \
	  } \
        }

void
save_file(CF *arg_file)
    /* Store the informations relatives to the executions
     * of tasks at a defined frequency of system's running time */
{
    CF *file = NULL;
    CL *line = NULL;
    FILE *f = NULL;
    CF *start_file = NULL;
    env_t *env = NULL;

    if (arg_file != NULL)
	start_file = arg_file;
    else
	start_file = file_base;
    	
    
    for (file = start_file; file; file = file->cf_next) {

	debug("Saving %s...", file->cf_user);

	/* open file */
	if ( (f = fopen(file->cf_user, "w")) == NULL ) {
	    error_e("Could not open %s", file->cf_user);
	    goto next_file;
	}

	/* chown the file to root:root : this file should only be read and
	 * modified by fcron (not fcrontab) */
	if (fchown(fileno(f), ROOTUID, ROOTGID) != 0)
	    error_e("Could not fchown \"%s\"", file->cf_user);

	/* save file : */

	/* put program's version : it permit to daemon not to load
	 * a file which he won't understand the syntax, for exemple
	 * a file using a depreciated format generated by an old fcrontab,
	 * if the syntax has changed */
	/* an binary fcrontab *must* start by such a header */
	Save_lint(f, S_HEADER_T, S_FILEVERSION );

	/* put the user's name : needed to check if his uid has not changed */
	/* S_USER_T *must* be the 2nd field of a binary fcrontab */
 	Save_str(f, S_USER_T, file->cf_user);

	/* put the time & date of saving : this is use for calcutating 
	 * the system down time. As it is a new file, we set it to 0 */
	/* S_USER_T *must* be the 3rd field of a binary fcrontab */
	Save_lint(f, S_TIMEDATE_T, now);

	/* Save the time diff between local (real) and system hour (if any) */
	if ( file->cf_tzdiff != 0 )
	    Save_lint(f, S_TZDIFF_T, file->cf_tzdiff);

	/*   env variables, */
	for (env = file->cf_env_base; env; env = env->e_next)
	    Save_str(f, S_ENVVAR_T, env->e_val);
	
	/*   then, lines. */
	for (line = file->cf_line_base; line; line = line->cl_next) {

	    /* this ones are saved for every lines */
	    Save_str(f, S_SHELL_T, line->cl_shell);
	    Save_str(f, S_RUNAS_T, line->cl_runas);
	    Save_str(f, S_MAILTO_T, line->cl_mailto);
	    Save_strn(f, S_OPTION_T, line->cl_option, OPTION_SIZE);

	    /* the following are saved only if needed */
	    if ( is_volatile(line->cl_option) && is_freq(line->cl_option) ) {
		Save_lint(f, S_FIRST_T, line->cl_first);
	    }
	    else
		Save_lint(f, S_NEXTEXE_T, line->cl_nextexe);
	    if ( line->cl_numexe )
		Save_strn(f, S_NUMEXE_T, &line->cl_numexe, 1);
	    if ( is_lavg(line->cl_option) )
		Save_strn(f, S_LAVG_T, line->cl_lavg, LAVG_SIZE);
	    if ( line->cl_until > 0 )
		Save_lint(f, S_UNTIL_T, line->cl_until);
	    if ( line->cl_nice != 0 )
		Save_strn(f, S_NICE_T, &line->cl_nice, 1);
	    if ( line->cl_runfreq > 0 ) {
		Save_lint(f, S_RUNFREQ_T, line->cl_runfreq);
		Save_lint(f, S_REMAIN_T, line->cl_remain);
	    }
		     
	    if ( is_freq(line->cl_option) ) {
		/* save the frequency to run the line */
		Save_lint(f, S_TIMEFREQ_T, line->cl_timefreq)
	    }
	    else {
		/* save the time and date bit fields */
		Save_strn(f, S_MINS_T, line->cl_mins, bitstr_size(60));
		Save_strn(f, S_HRS_T, line->cl_hrs, bitstr_size(24));
		Save_strn(f, S_DAYS_T, line->cl_days, bitstr_size(32));
		Save_strn(f, S_MONS_T, line->cl_mons, bitstr_size(12));
		Save_strn(f, S_DOW_T, line->cl_dow, bitstr_size(8));
	    }

	    /* This field *must* be the last of each line */
	    Save_type(f, S_ENDLINE_T);
	}

	fclose(f);

	if (arg_file != NULL)
	    /* we have to save only a single file */
	    break ;

      next_file:
    }
}
