
/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000 Thibault Godouet <fcron@free.fr>
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

 /* $Id: conf.c,v 1.14 2000-06-25 19:57:37 thib Exp $ */

#include "fcron.h"

int read_file(const char *file_name, CF *cf);
char *read_str(FILE *f, char *buf, int max);
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
	if ( f->cf_running > 0 ) {
	    wait_all( &f->cf_running );
	    save_file(f, NULL);
	}
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
	explain("update configuration from " FCRONTABS );
    else
	explain("update configuration from '%s'", dir_name);

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

    /* begin by adding normal files, if any, to database */
    for (list_cur = file_list; list_cur; list_cur = list_cur->next ) {
	if ( getpwnam(list_cur->str) ) {
	    explain("adding file '%s'", list_cur->str);	    
	    synchronize_file(list_cur->str);
	}
	else
	    error("ignoring file '%s' : not in passwd file.", list_cur->str);
    }

    /* then remove files which are no longer wanted */
    for (list_cur = rm_list; list_cur; list_cur = list_cur->next ) {
	explain("removing file '%s'", list_cur->str + 3);
	delete_file(list_cur->str + 3);  /* len("rm.") = 3 */
	remove(list_cur->str + 3);
	remove(list_cur->str);
    }
    
    /* finally add new files */
    for (list_cur = new_list; list_cur; list_cur = list_cur->next ) {
	if ( getpwnam(list_cur->str + 4) ) { /* len("new.") = 4 */
	    explain("adding new file '%s'", list_cur->str + 4);
	    synchronize_file(list_cur->str);  
	}
	else
	    error("ignoring file '%s' : not in passwd file.", 
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
		free(cur_f);
		return;
	    }

	    /* set cf_user field */
	    cur_f->cf_user = strdup2(user);

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
				  size)==0 )
			) ) {
			
			if ( new_l->cl_runfreq == old_l->cl_runfreq ) 
			    new_l->cl_remain = old_l->cl_remain;
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
	    save_file(cur_f, user);

	    /* delete new.user file */
	    if ( remove(file_name) != 0 )
		error_e("could not remove %s", file_name);

	}

	else {
	    /* no old version exist in database : load this file
	     * as a normal file, but change its name */
	    user = (file_name + 4);
	
	    Alloc(cur_f, CF);

	    if ( read_file(file_name, cur_f) != 0 ) {
		/* an error as occured */
		free(cur_f);
		return;
	    }

	    /* set cf_user field */
	    cur_f->cf_user = strdup2(user);

	    /* insert the file in the list */
	    cur_f->cf_next = file_base;
	    file_base = cur_f;

	    /* save as a normal file */
	    save_file(cur_f, user);

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
	    free(cur_f);
	    return;
	}

	/* set cf_user field */
	user = file_name;
	cur_f->cf_user = strdup2(user);

	/* insert the file in the list */
	cur_f->cf_next = file_base;
	file_base = cur_f;

    }

}


char *
read_str(FILE *f, char *buf, int max)
    /* return a pointer to string read from file f
     * if it is non-zero length */
{
    int i;

    for (i = 0; i < max; i++)
	if ( (buf[i] = fgetc(f)) == '\0')
	    break;

    if ( strlen(buf) == 0 )
	return NULL;
    else
	return strdup2(buf);

}


int
read_file(const char *file_name, CF *cf)
    /* read a formated fcrontab.
       return 1 on error, 0 otherwise */
{
    FILE *ff = NULL;
    CL *cl = NULL;
    env_t *env = NULL;
    char buf[LINE_LEN];
    time_t t_save = 0;
    time_t slept = 0;
    char c;
    int i;
    char *user = NULL;
    char *runas = NULL;
    struct passwd *pas = NULL;


    /* open file */
    if ( (ff = fopen(file_name, "r")) == NULL ) {
	error_e("Could not read %s", file_name);
	return 1;
    }

    debug("User %s Entry", file_name);
    bzero(buf, sizeof(buf));

    /* get version of fcrontab file: it permit to daemon not to load
     * a file which he won't understand the syntax, for exemple
     * a file using a depreciated format generated by an old fcrontab,
     * if the syntax has changed */
    if (fgets(buf, sizeof(buf), ff) == NULL ||
	strncmp(buf, "fcrontab-"FILEVERSION"\n",
		sizeof("fcrontab-"FILEVERSION"\n")) != 0) {

	error("File '%s' is not valid: ignored.", file_name);
	error("Maybe this file has been generated by an old version"
	      " of fcron. In this case, you should install it again"
	      " using fcrontab.");
	return 1;
    }    

    /* get the owner's name */
    user = read_str(ff, buf, sizeof(buf));


    /* get the time & date of the saving */
    /* a new file generated by fcrontab has t_save set to 0 */
    if ( fscanf(ff, "%ld", &t_save) != 1 )
	error("could not get time and date of saving");
	
    slept = now - t_save;


    /* check if there is a mailto field in file */
    if ( (c = getc(ff)) == 'm' ) {
	/* there is one : read it */

	for (i = 0; i < sizeof(buf); i++)
	    if ( (buf[i] = fgetc(ff)) == '\0')
		break;
	cf->cf_mailto = strdup2(buf);

	debug("  MailTo: '%s'", cf->cf_mailto);
    } else
	/* if there is no mailto field, the first letter is not a 'm',
	 * but a 'e' : there is no need to lseek */
	;
	    
    /* read env variables */
    Alloc(env, env_t);
    while( (env->e_name = read_str(ff, buf, sizeof(buf))) != NULL ) {
	env->e_val = read_str(ff, buf, sizeof(buf));
	debug("  Env: '%s=%s'", env->e_name, env->e_val );
	
	env->e_next = cf->cf_env_base;
	cf->cf_env_base = env;
	Alloc(env, env_t);
    }
    /* free last alloc : unused */
    free(env);
	
    /* read lines */
    Alloc(cl, CL);
    while ( fread(cl, sizeof(CL), 1, ff) == 1 ) {

	cl->cl_shell = read_str(ff, buf, sizeof(buf));

	if ( is_td(cl->cl_option) ) {
	    /* set the time and date of the next execution  */
	    if ( cl->cl_nextexe <= now ) {
		if ( is_bootrun(cl->cl_option) && t_save != 0) {
		    if ( --cl->cl_remain > 0 ) {
			debug("    cl_remain: %d", cl->cl_remain);
		    }
		    else {
			cl->cl_remain = cl->cl_runfreq;
			debug("   boot-run %s", cl->cl_shell);
			set_serial_once(cl->cl_option);
			cl->cl_pid = -1;
		    }
		}
		set_next_exe(cl, 1);
	    }
	    else
		insert_nextexe(cl);
	} else {
	    cl->cl_nextexe += slept;
	    insert_nextexe(cl);
	}	    

	if ( cl->cl_pid == -1 ) {
	    cl->cl_pid = 0;
	    add_serial_job(cl);
	}

	/* set the uid  */
	if ( is_runas(cl->cl_option) ) {
	    struct passwd *p = NULL;
	    runas = read_str(ff, buf, sizeof(buf));
	    if ( (p = getpwnam(runas)) == NULL )
		die_e("could not getpwnam() %s", runas);
	    cl->cl_runas = p->pw_uid;
	}
	else {
	    if ( pas == NULL && (pas = getpwnam(user)) == NULL )
		die_e("could not getpwnam() %s", user);
	    cl->cl_runas = pas->pw_uid;
	}


	/* check if the task has not been stopped during execution */
	if (cl->cl_pid > 0) {
	    /* job has been stopped during execution :
	     * launch it again */
	    warn("job '%s' has not terminated : executed again now.",
		 cl->cl_shell);
	    cl->cl_pid = 0;
	    add_serial_job(cl);
	}
	    

	if (debug_opt) {
	    struct tm *ftime;
	    ftime = localtime( &(cl->cl_nextexe) );
	    debug("  cmd '%s' next exec %d/%d/%d wday:%d %02d:%02d",
		  cl->cl_shell, (ftime->tm_mon + 1), ftime->tm_mday,
		  (ftime->tm_year + 1900), ftime->tm_wday,
		  ftime->tm_hour, ftime->tm_min); 
	} 
	

	cl->cl_next = cf->cf_line_base;
	cf->cf_line_base = cl;
	cl->cl_file = cf;
	Alloc(cl, CL);
    }
    /* free last calloc : unused */
    free(cl);
    
    fclose(ff);

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

    file = file_base;
    while ( file != NULL) {
	if (strcmp(user_name, file->cf_user) == 0) {

	    /* free lines */
	    cur_line = file->cf_line_base;
	    while ( (line = cur_line) != NULL) {
		cur_line = line->cl_next;

		/* remove line from the lists */
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
		free(line);
	    }
	    /* delete_file() MUST remove only the first occurrence :
	     * this is needed by synchronize_file() */
	    break ;
	} else {
	    prev_file = file;
	    file = file->cf_next;
	}
    }
    
    if (file == NULL)
	/* file not in list */
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
	free(env->e_name);
	free(env->e_val);
	free(env);
    }

    /* finally free file itself */
    free(file->cf_user);
    free(file->cf_mailto);
    free(file);

}


void
save_file(CF *file, char *path)
    /* Store the informations relatives to the executions
     * of tasks at a defined frequency of system's running time */
{
    CF *cf = NULL;
    CL *cl = NULL;
    FILE *f = NULL;
    char check[FNAME_LEN];
    CF *start = NULL;
    int fd = 0;
    env_t *env = NULL;
    struct passwd *pas = NULL;

    if (file != NULL)
	start = file;
    else
	start = file_base;
    	
    
    for (cf = start; cf; cf = cf->cf_next) {

	debug("Saving %s...", cf->cf_user);

	/* create a file in order to detect unattended stop */
	snprintf(check, sizeof(check), "%s.saving", cf->cf_user);
	fd = open (check, O_CREAT);

	/* open file for writing */
	if ( path == NULL ) {
	    if ( (f = fopen(cf->cf_user, "w")) == NULL )
		error_e("save");
	}
	else
	    if ( (f = fopen(path, "w")) == NULL )
		error_e("save");

	/* save file : */

	/* put version of frontab file: it permit to daemon not to load
	 * a file which he won't understand the syntax, for exemple
	 * a file using a depreciated format generated by an old fcrontab,
	 * if the syntax has changed */
	fprintf(f, "fcrontab-" FILEVERSION "\n");

	/* put the user's name : it is used to check his uid has not changed */
 	fprintf(f, "%s%c", cf->cf_user, '\0');

	/* put the time & date of saving : this is use for calcutating 
	 * the system down time */
	fprintf(f, "%ld", now);

	/*   mailto, */
	if ( cf->cf_mailto != NULL ) {
	    fprintf(f, "m");
	    fprintf(f, "%s%c", cf->cf_mailto, '\0');
	} else
	    fprintf(f, "e");

	/*   env variables, */
	for (env = cf->cf_env_base; env; env = env->e_next) {
	    fprintf(f, "%s%c", env->e_name, '\0');
	    fprintf(f, "%s%c", env->e_val, '\0');
	}
	fprintf(f, "%c", '\0');

	/*   finally, lines. */
	for (cl = cf->cf_line_base; cl; cl = cl->cl_next) {
	    if ( fwrite(cl, sizeof(CL), 1, f) != 1 )
		error_e("save");
	    fprintf(f, "%s%c", cl->cl_shell, '\0');
	    if ( is_runas(cl->cl_option) ) {
		if ( (pas = getpwuid(cl->cl_runas)) == NULL )
		    die_e("could not getpwuid() %d", cl->cl_runas);
		fprintf(f, "%s%c", pas->pw_name, '\0');
	    }

	}
    
	fclose(f);

	close(fd);
	remove(check);
	
	if (file != NULL)
	    break ;

    }
}
