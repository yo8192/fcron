
/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000 Thibault Godouet <sphawk@free.fr>
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

 /* $Id: fileconf.c,v 1.2 2000-05-15 18:28:44 thib Exp $ */

#include "fcrontab.h"

int get_line(char *str, size_t size, FILE *file, int *line);
char *get_time(char *ptr, time_t *time, int line, char *file_name);
char *read_num(char *ptr, int *num, int max, const char **names);
void read_freq(char *ptr, CF *cf, int line, char *file_name);
void read_arys(char *ptr, CF *cf, int line, char *file_name);
char *read_field(char *ptr, bitstr_t *ary, int max, const char **names,
	       int line, char *file_name);
void read_env(char *ptr, CF *cf, int line);
char *get_string(char *ptr);


/* warning : all names must have the same length */
const char *dows_ary[] = {
    "sun", "mon", "tue", "wed", "thu", "fri", "sat",
    NULL
};

/* warning : all names must have the same length */
const char *mons_ary[] = {
    "jan", "feb", "mar", "apr", "may", "jun", 
    "jul", "aug", "sep", "oct", "nov", "dec",
    NULL
};


char *
get_string(char *ptr)
    /* read string pointed by ptr, remove blanks and manage
     * string placed in quotes */
    /* return NULL on mismatched quotes */
{
    char quote = 0;
    int length = 0;

    if ( *ptr == '\"' || *ptr == '\'' ) {
	quote = *ptr;
	ptr++;
    }

    length = remove_blanks(ptr);

    if ( quote != 0 ) {
	if ( *(ptr + length - 1) == quote )
	    *(ptr + length - 1) = '\0';
	else
	    /* mismatched quotes */
	    return NULL;
    }
    

    return strdup2(ptr);

}


int
get_line(char *str, size_t size, FILE *file, int *line)
    /* similar to fgets, but increase line if necessary,
     * and continue over an "\" followed by an "\n" char */
{
    size_t size_max = size - 1 ;
    register int i=0;

    while (i < size_max ) {

	switch ( *(str + i) = getc(file) ) {

	case '\n':
	    /* check if the \n char is preceded by a "\" char : 
	     *  in this case, suppress the "\", don't copy the \n,
	     *  and continue */
	    if ( *(str + i - 1) == '\\') {
		i--;
		(*line)++;
		continue;
	    }
	    else {
		*(str + i) = '\0';
		return OK;
	    }
	    break;
		
	case EOF:
	    *(str + i) = '\0';
	    /* we couldn't return EOF ( equal to ERR by default ) 
	     * nor ERR, which is used for another error */
	    return 999;

	default:
	    i++;

	}

    }

    /* line is too long : goto next line and return ERR */
    while ((*str = getc(file)) != '\n' && *str != EOF )
	;
    (*line)++;
    return ERR;

}

int
read_file(char *file_name, char *user)
    /* read file "name" and append CF list */
{
    CF *cf = NULL;
    FILE *file = NULL;
    char buf[LINE_LEN];
    int max_lines;
    int line = 1;
    int max_entries = MAXLINES;
    int entries=0;
    char *ptr = NULL;
    int ret;
    
    bzero(buf, sizeof(buf));

    /* open file */

    /* check if user is allowed to read file */
    if ( access(file_name, R_OK) != 0 )
	die_e("User %s can't read file '%s'", user, file_name);
    else if ( (file = fopen(file_name, "r")) == NULL ) {
	fprintf(stderr, "Could not open '%s': %s\n", file_name,
		strerror(errno));
	return ERR;
    }

    Alloc(cf, CF);

    if ( debug_opt )
	fprintf(stderr, "FILE %s\n", file_name);

    if (strcmp(user, "root") == 0)
	max_entries = 65535;

    /* max_lines acts here as a security counter to avoid endless loop. */
    max_lines = max_entries * 10;

    while ( entries <= max_entries && line <= max_lines ) {

	if ( (ret = get_line(buf, sizeof(buf), file, &line)) == OK)
	    ;
	else if ( ret == ERR ) {
	    fprintf(stderr, "Line %d of %s is too long (more than %d):"
		    " skipping line.\n",line, file_name, sizeof(buf));
	    continue;
	} else
	    /* EOF : no more lines */
	    break;
	
	ptr = buf;
	Skip_blanks(ptr);

	switch(*ptr) {
	case '#':
	case '\0':
	    /* comments or empty line: skipping */
	    line++;
	    continue;
	case '@':
	    if (debug_opt)
		fprintf(stderr, "      %s\n", buf);
	    read_freq(ptr, cf, line, file_name);
	    entries++;
	    break;
	case '&':
	    if (debug_opt)
		fprintf(stderr, "      %s\n", buf);
	    read_arys(ptr, cf, line, file_name);
	    entries++;
	    break;
	default:
	    if ( isdigit(*ptr) || *ptr == '*' ) {
		if (debug_opt)
		    fprintf(stderr, "      %s\n", buf);
		read_arys(ptr, cf, line, file_name);
		entries++;
	    } else
		read_env(ptr, cf, line);
	}

	line++;	

    }

    cf->cf_user = user;
    cf->cf_next = file_base;
    file_base = cf;

    fclose(file);
    
    return OK;

}

void
read_env(char *ptr, CF *cf, int line)
    /* append env variable list.
     * (remove blanks) */
{
    char name[ENV_LEN];
    env_t *env = NULL;
    int j=0;
    char *val = NULL;

    bzero(name, sizeof(name));

    /* copy env variable's name */
    while ( isalnum(*ptr) && *ptr != '=' && j < sizeof(name)) {
	name[j++] = *ptr;
	ptr++;
    }
    name[j] = '\0';

    /* skip '=' and spaces around */
    while ( isspace(*ptr) || *ptr == '=' )
	ptr++;

    /* get value */
    if ( ( val = get_string(ptr)) == NULL ) {
	fprintf(stderr, "Error at line %d (mismatched"
		" quotes): skipping line.\n", line);
	return;
    }

    if (debug_opt)
	fprintf(stderr, "  Env : '%s=%s'\n", name, val);

    /* we ignore USER's assignment */
    if ( strcmp(name, "USER") == 0 )
	return;

    /* the MAILTO assignment is, in fact, an fcron option :
     *  we don't store it in the same way. */
    if ( strcmp(name, "MAILTO") == 0 )
	cf->cf_mailto = val;
    
    else {

	Alloc(env, env_t);	

	env->e_name = strdup2(name);
	env->e_val = val;
	env->e_next = cf->cf_env_base;
	cf->cf_env_base = env;
    }
    
    return;

}


char *
get_time(char *ptr, time_t *time, int line, char *file_name )
    /* convert time read in string in time_t format */
{
    time_t sum;
    
    while( (*ptr != ' ') && (*ptr != '\t') && (*ptr != '\0') ) { 

	sum = 0;

	while ( isdigit(*ptr) ) {
	    sum *= 10;
	    sum += *ptr - 48;
	    ptr++;
	}

	/* interpret multipliers */
	switch (*ptr) {
	case 'm':               /* months */
	    sum *= 4;
	case 'w':               /* weeks */
	    sum *= 7;
	case 'd':               /* days */
	    sum *= 24;
	case 'h':               /* hours */
	    sum *= 60;
	    ptr++;
	default:                /* minutes */
	    sum *= 60;
	    
	}

	*time += sum;

    }

    Skip_blanks(ptr);
    return ptr;
}



void
read_freq(char *ptr, CF *cf, int line, char *file_name)
    /* read a freq entry, and append a line to cf */
{
    CL *cl=NULL;
    
    Alloc(cl, CL);

    ptr++;
    /* get cl_remain field */
    ptr = get_time(ptr, &(cl->cl_remain), line, file_name);

    Skip_blanks(ptr);

    /* then cl_timefreq */
    ptr = get_time(ptr, &(cl->cl_timefreq), line, file_name);
    if ( cl->cl_timefreq == 0) {
	fprintf(stderr, "Error at line %d of file %s (no freq"
		" specified): skipping line.\n", line, file_name);
	free(cl);
	return;
    }

    if ( cl->cl_remain == 0 )
	/* cl_remain is not specified : set it to cl_timefreq value */
	cl->cl_remain = cl->cl_timefreq;

    /* get cl_shell field ( remove trailing blanks ) */
    if ( (cl->cl_shell = get_string(ptr)) == NULL ) {
	fprintf(stderr, "Error at line %d of file %s (mismatched"
		" quotes): skipping line.\n", line, file_name);
	free(cl);
	return;
    }

    cl->cl_next = cf->cf_line_base;
    cf->cf_line_base = cl;

    if ( debug_opt )
	fprintf(stderr, "  Cmd '%s', timefreq %ld, remain %ld\n",
		cl->cl_shell, cl->cl_timefreq, cl->cl_remain);

}



#define R_field(ptr, ary, max, aryconst, descrp) \
  if((ptr = read_field(ptr, ary, max, aryconst, line, file_name)) == NULL) { \
      if (debug_opt) \
          fprintf(stderr, "\n"); \
      fprintf(stderr, "Error while reading " descrp " field line %d" \
             " of file %s: ignoring line.\n", line, file_name); \
      free(cl); \
      return; \
  }

void
read_arys(char *ptr, CF *cf, int line, char *file_name)
    /* read a run freq number plus a normal fcron line */
{
    CL *cl = NULL;

    Alloc(cl, CL);


    /* set cl_remain if not specified or 
     * if set to 1 to skip unnecessary tests */
    if ( *ptr != '&' )
	/* cl_remain not specified : set it to 0 */
	cl->cl_remain = 0;
    else {
	ptr++;
	/* get remain number */
	while ( isdigit(*ptr) ) {	    
	    cl->cl_remain *= 10;
	    cl->cl_remain += *ptr - 48;
	    ptr++;
	}

	Skip_blanks(ptr);
    }

    cl->cl_runfreq = cl->cl_remain;

    if (debug_opt)
	fprintf(stderr, "     ");

    /* get the fields (check for errors) */
    R_field(ptr, cl->cl_mins, 60, NULL, "minutes");
    R_field(ptr, cl->cl_hrs, 24, NULL, "hours");
    R_field(ptr, cl->cl_days, 32, NULL, "days");
    R_field(ptr, cl->cl_mons, 12, mons_ary, "months");
    R_field(ptr, cl->cl_dow, 8, dows_ary, "days of week");

    if (debug_opt)
	/* if debug_opt is set, we print informations in read_field function,
	 *  but no end line : we print it here */
	fprintf(stderr, " remain %ld\n", cl->cl_remain);

    /* get the shell command (remove trailing blanks) */
    if ( (cl->cl_shell = get_string(ptr)) == NULL ) {
	fprintf(stderr, "Error at line %d of file %s (mismatched"
		" quotes): skipping line.\n", line, file_name);
	free(cl);
	return;
    }

    cl->cl_next = cf->cf_line_base;
    cf->cf_line_base = cl;

    if ( debug_opt )
	fprintf(stderr, "  Cmd '%s'\n", cl->cl_shell);


}

char *
read_num(char *ptr, int *num, int max, const char **names)
    /* read a string's number and return it under int format.
     *  Also check if that number is less than max */
{
    *num = 0;

    if ( isalpha(*ptr) ) {
	int i;

	/* set string to lower case */
	for ( i = 0; i < strlen(names[0]); i++ )
	    *(ptr+i) = tolower( *(ptr+i) );

	for (i = 0; names[i]; ++i)
	    if (strncmp(ptr, names[i], strlen(names[i])) == 0) {
		*num = i;
		ptr += strlen(names[i]);
		return ptr;
		break;	    
	    }

	/* string is not in name list */
	return NULL;

    } else {

	if ( max == 12 )
	    /* month are defined by user from 1 to 12 */
	    max = 13;

	while ( isdigit(*ptr) ) { 
	    *num *= 10; 
	    *num += *ptr - 48; 

	    if (*num >= max) 
		return NULL;

	    ptr++; 

	} 

	if ( max == 13 )
	    /* this number is part of the month field.
	     * user set it from 1 to 12, but we manage it internally
	     * as a number from 0 to 11 : we remove 1 to *num */
	    *num = *num - 1;

    }

    return ptr;
}


char *
read_field(char *ptr, bitstr_t *ary, int max, const char **names,
	   int line, char *file_name)
    /* read a field like "2,5-8,10-20/2,21-30~25" and fill ary */
{
    int start = 0;
    int stop = 0;
    int step = 0;
    int rm = 0;
    int i = 0;

    while ( (*ptr != ' ') && (*ptr != '\t') && (*ptr != '\0') ) {
	
	start = stop = step = 0 ;

	/* there may be a "," */
	if ( *ptr == ',' )
	    ptr++;

	if ( *ptr == '*' ) {
	    /* we have to fill everything (may be modified by a step ) */
	    start = 0;
	    stop = max - 1;
	    ptr++;
	} else {

	    if ( (ptr = read_num(ptr, &start, max, names)) == NULL )
		return NULL;

	    if (*ptr == ',' || *ptr == ' ' || *ptr == '\t') {
		/* this is a single number : set up array and continue */
		if (debug_opt)
		    fprintf(stderr, " %d", start);
		bit_set(ary, start);
		continue;
	    }

	    /* check for a dash */
	    else if ( *ptr == '-' ) {
		ptr++;
		if ( (ptr = read_num(ptr, &stop, max, names)) == NULL )
		    return NULL;
	    } else
		/* syntax error */
		return NULL;

	}

	/* check for step size */
	if ( *ptr == '/' ) {
	    ptr++;
	    if ((ptr = read_num(ptr, &step, max, names)) == NULL || step == 0)
		return NULL;
	} else
	    /* step undefined : default is 0 */
	    step = 1;
	
	/* fill array */	
	if (debug_opt)
	    fprintf(stderr, " %d-%d/%d", start, stop, step);

	for (i = start;  i <= stop;  i += step)
	    bit_set(ary, i);

	/* finally, remove unwanted values */
	while ( *ptr == '~' ) {
	    ptr++;
	    rm = 0;
	    if ( (ptr = read_num(ptr, &rm, max, names)) == NULL )
		return NULL;

	    if (debug_opt)
		fprintf(stderr, " ~%d", rm);	    
	    bit_clear(ary, rm);

	    /* if we remove one value of Sunday, remove the other */
	    if (max == 8 && rm == 0) {
		bit_clear(ary, 7);
	    if (debug_opt)
		fprintf(stderr, " ~%d", 7);	    
	    }
	    else if (max == 8 && rm == 7) {
		bit_clear(ary, 0);
	    if (debug_opt)
		fprintf(stderr, " ~%d", 0);	    	    
	    }

	}

    }

    /* Sunday is both 0 and 7 : if one is set, set the other */
    if ( max == 8 ) {
	if ( bit_test(ary, 0) )
	    bit_set(ary, 7);
	else if ( bit_test(ary, 7) )
	    bit_set(ary, 0);
    }

    Skip_blanks(ptr);

    if (debug_opt)
	fprintf(stderr, " #");

    return ptr;
}


void
delete_file(const char *user_name)
    /* free a file if user_name is not null 
     *   otherwise free all files */
{
    CF *file = NULL;
    CF *prev_file = NULL;
    CL *line = NULL;
    CL *cur_line = NULL;
    env_t *env = NULL;
    env_t *cur_env = NULL;

    file = file_base;
    while ( file != NULL) {
	if (strcmp(user_name, file->cf_user) == 0) {

	    /* free lines */
	    cur_line = file->cf_line_base;
	    while ( (line = cur_line) != NULL) {
		cur_line = line->cl_next;
		free(line->cl_shell);
		free(line);
	    }
	    break ;

	} else {
	    prev_file = file;
	    file = file->cf_next;
	}
    }
    
    if (file == NULL)
	/* file not in list */
	return;
    
    /* remove file from list */
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
save_file(char *path)
    /* Store the informations relatives to the executions
     * of tasks at a defined frequency of time of system's
     * run */
{
    CF *file = NULL;
    CL *line = NULL;
    FILE *f = NULL;
    env_t *env = NULL;

    if (debug_opt)
	fprintf(stderr, "Saving ...\n");
    
    for (file = file_base; file; file = file->cf_next) {

	/* open file */
	if ( (f = fopen(path, "w")) == NULL )
	    perror("save");

	/* save file : */

	/* put program's version : it permit to daemon not to load
	 * a file which he won't understand the syntax, for exemple
	 * a file using a depreciated format generated by an old fcrontab,
	 * if the syntax has changed */
	fprintf(f, "fcrontab-" FILEVERSION "\n");

	/*   mailto, */
	if ( file->cf_mailto != NULL ) {
	    fprintf(f, "m");
	    fprintf(f, "%s%c", file->cf_mailto, '\0');
	} else
	    fprintf(f, "e");

	/*   env variables, */
	for (env = file->cf_env_base; env; env = env->e_next) {
	    fprintf(f, "%s%c", env->e_name, '\0');
	    fprintf(f, "%s%c", env->e_val, '\0');
	}
	fprintf(f, "%c", '\0');

	/*   finally, lines. */
	for (line = file->cf_line_base; line; line = line->cl_next) {
	    if ( fwrite(line, sizeof(CL), 1, f) != 1 )
		perror("save");
	    fprintf(f, "%s%c", line->cl_shell, '\0');
	}
    
	fclose(f);

    }
}
