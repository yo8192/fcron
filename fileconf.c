
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

 /* $Id: fileconf.c,v 1.49 2001-07-04 16:15:54 thib Exp $ */

#include "fcrontab.h"
#include "fileconf.h"

char *get_string(char *ptr);
int get_line(char *str, size_t size, FILE *file);
char *get_time(char *ptr, time_t *time);
char *get_num(char *ptr, int *num, int max, short int decimal,
	      const char **names);
char *get_nice(char *ptr, int *nice);
char *get_bool(char *ptr, int *i);
char *read_field(char *ptr, bitstr_t *ary, int max, const char **names);
void read_freq(char *ptr, CF *cf);
void read_arys(char *ptr, CF *cf);
void read_period(char *ptr, CF *cf);
void read_env(char *ptr, CF *cf);
char *read_opt(char *ptr, CL *cl);
char *check_username(char *ptr, CF *cf, CL *cl);


char need_correction;
CL default_line;    /* default options for a line */
char *file_name;
int line;

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
	else {
	    /* mismatched quotes */
	    need_correction = 1;
	    return NULL;
	}
    }
    

    return strdup2(ptr);

}


int
get_line(char *str, size_t size, FILE *file)
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
	    if ( i > 0 && *(str + i - 1) == '\\') {
		i--;
		line++;
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
    line++;
    need_correction = 1;
    return ERR;

}

int
read_file(char *filename, char *user)
    /* read file "name" and append CF list */
{
    CF *cf = NULL;
    FILE *file = NULL;
    char buf[LINE_LEN];
    int max_lines;
    int max_entries = MAXENTRIES;
    int entries=0;
    char *ptr = NULL;
    int ret;
    
    bzero(buf, sizeof(buf));
    bzero(&default_line, sizeof(CL));
    need_correction = 0;
    line = 1;
    file_name = filename;

    /* open file */

    /* check if user is allowed to read file */
    if ( access(file_name, R_OK) != 0 )
	die_e("User %s can't read file \"%s\"", user, file_name);
    else if ( (file = fopen(file_name, "r")) == NULL ) {
	fprintf(stderr, "Could not open \"%s\": %s\n", file_name,
		strerror(errno));
	return ERR;
    }

    Alloc(cf, CF);
    cf->cf_user = strdup2(user);
    default_line.cl_file = cf;
    default_line.cl_runas = strdup2(user);
    default_line.cl_mailto = strdup2(user);
    set_default_opt(default_line.cl_option);

    if ( debug_opt )
	fprintf(stderr, "FILE %s\n", file_name);

    if (strcmp(user, "root") == 0)
	max_entries = 65535;

    /* max_lines acts here as a security counter to avoid endless loop. */
    max_lines = (max_entries * 10) + 10;

    while ( entries <= max_entries && line <= max_lines ) {

	if ( (ret = get_line(buf, sizeof(buf), file)) == OK)
	    ;
	else if ( ret == ERR ) {
	    fprintf(stderr, "%s:%d: Line is too long (more than %d):"
		    " skipping line.\n", file_name, line, sizeof(buf));
	    continue;
	} else
	    /* EOF : no more lines */
	    break;
	
	ptr = buf;
	Skip_blanks(ptr);

	if (debug_opt && *ptr != '#' && *ptr != '\0')
	    fprintf(stderr, "      %s\n", buf);

	switch(*ptr) {
	case '#':
	case '\0':
	    /* comments or empty line: skipping */
	    line++;
	    continue;
	case '@':
	    read_freq(ptr, cf);
	    entries++;
	    break;
	case '&':
	    read_arys(ptr, cf);
	    entries++;
	    break;
	case '%':
	    read_period(ptr, cf);
	    entries++;
	    break;
	case '!':
	    ptr = read_opt(ptr, &default_line);
	    if ( ptr != NULL && *ptr != '\0' ) {
		fprintf(stderr, "%s:%d: Syntax error: string \"%s\" ignored\n",
			file_name, line, ptr);
		need_correction = 1;
	    }
	    break;
	default:
	    if ( isdigit( (int) *ptr) || *ptr == '*' ) {
		read_arys(ptr, cf);
		entries++;
	    } else
		read_env(ptr, cf);
	}

	line++;	

    }

    if (entries == max_entries) {
	error("%s:%d: maximum number of entries (%d) has been reached by %s",
	      file_name, line, user);
	fprintf(stderr, "Anything after this line will be ignored\n");
    }
    else if (line == max_lines)
	error("%s:%d: maximum number of lines (%d) has been reached by %s",
	      file_name, line, user);

    cf->cf_next = file_base;
    file_base = cf;

    fclose(file);
    
    free(default_line.cl_runas);
    free(default_line.cl_mailto);

    if ( ! need_correction )
	return OK;
    else
	return 2;

}

void
read_env(char *ptr, CF *cf)
    /* append env variable list.
     * (remove blanks) */
{
    char name[LINE_LEN];
    env_t *env = NULL;
    int j=0;
    char *val = NULL;

    bzero(name, sizeof(name));

    /* copy env variable's name */
    while ( (isalnum( (int) *ptr) || *ptr == '_') && *ptr != '=' 
	    && ! isspace( (int) *ptr) && j < sizeof(name) ) {
	name[j++] = *ptr;
	ptr++;
    }
    name[j] = '\0';

    if ( name == '\0' )
	goto error;

    /* skip '=' and spaces around */
    while ( isspace( (int) *ptr) )
	ptr++;

    /* if j == 0 name is a zero length string */
    if ( *ptr++ != '=' || j == 0 )
	goto error;

    while ( isspace( (int) *ptr) )
	ptr++;

    /* get value */
    if ( ( val = get_string(ptr)) == NULL ) {
	fprintf(stderr, "%s:%d: Mismatched  quotes: skipping line.\n",
		file_name, line);
	need_correction = 1;
	return;
    }

    if (debug_opt)
	fprintf(stderr, "  Env : '%s=%s'\n", name, val);

    /* we ignore USER's assignment */
    if ( strcmp(name, "USER") == 0 ) {
	fprintf(stderr, "%s:%d: USER assignement is not allowed: ignored.\n",
		file_name, line);	
	return;
    }

    /* the MAILTO assignment is, in fact, an fcron option :
     *  we don't store it in the same way. */
    if ( strcmp(name, "MAILTO") == 0 ) {
	if ( strcmp(val, "\0") == 0 )
	    clear_mail(default_line.cl_option);
	else {
	    struct passwd *pass = NULL;
	    if ( (pass = getpwnam(val)) == 0 ) {
		fprintf(stderr, "%s:%d:MAILTO: \"%s\" is not in passwd :"
			" ignored\n", file_name, line, val);	
		need_correction = 1;
	    } else {
		Set(default_line.cl_mailto, val);
		set_mail(default_line.cl_option);
	    }
	}
	    
    }
    else {

	Alloc(env, env_t);	

	strncat(name, "=", sizeof(name) - strlen(name) - 1);
	env->e_val = strdup2( strncat(name,val,sizeof(name)-strlen(name)-1) );
	env->e_next = cf->cf_env_base;
	cf->cf_env_base = env;
    }
    
    return;

  error:
	fprintf(stderr, "%s:%d: Syntax error: skipping line.\n",
		file_name, line);
	need_correction = 1;
	return;

}


char *
get_nice(char *ptr, int *nice)
    /* read a nice value and put it in variable nice */
{
    char negative = 0;

    if ( *ptr == '-' ) {
	negative = 1;
	ptr++;
    }

    if ( (ptr = get_num(ptr, nice, 20, 0, NULL)) == NULL )
	return NULL;

    if ( negative == 1 ) {
	if (getuid() != 0) {
	    fprintf(stderr, "must be privileged to use a negative argument "
		    "with nice: set to 0\n");
	    need_correction = 1;
	    *nice = 0;
	}

	*nice *= (-1);
    }

    return ptr;

}


char *
get_bool(char *ptr, int *i)
    /* get a bool value : either true (1) or false (0)
     * return NULL on error */
{
    if ( *ptr == '1' )
	goto true;
    else if ( *ptr == '0' )
	goto false;
    else if ( strncmp(ptr, "true", 4) == 0 ) {
	ptr += 3;
	goto true;
    }
    else if ( strncmp(ptr, "yes", 3) == 0 ) {
	ptr += 2;
	goto true;
    }
    else if ( strncmp(ptr, "false", 5) == 0 ) {
	ptr += 4;
	goto false;
    }
    else if ( strncmp(ptr, "no", 2) == 0 ) {
	ptr += 1;
	goto false;
    }
    else
	return NULL;

  true:
    *i = 1;
    ptr++;
    return ptr;

  false:
    *i = 0;
    ptr++;
    return ptr;
    
}


char *
read_opt(char *ptr, CL *cl)
    /* read one or several options and fill in the field "option" */
{
    char opt_name[20];
    unsigned int i;
    char in_brackets;
    
#define Handle_err \
    { \
        fprintf(stderr, "%s:%d: Argument(s) for option \"%s\" not valid: " \
		"skipping end of line.\n", file_name, line, opt_name); \
        need_correction = 1; \
        return NULL; \
    }

    if ( *ptr == '!' )
	ptr++;

    do { 
	i = 0;
	bzero(opt_name, sizeof(opt_name));

	while ( isalnum( (int) *ptr)  && i < sizeof(opt_name))
	    opt_name[i++] = *ptr++;
    
	i = 1;
	in_brackets = 0;

	if ( *ptr == '(' ) {
	    in_brackets = 1;
	    ptr++;
	}

	/* global options for a file */
	if ( strcmp(opt_name, "tzdiff") == 0 ) {
	    char negative = 0;

	    if ( ! in_brackets )
		Handle_err;
	    if ( *ptr == '-' ) {
		negative = 1;
		ptr++;
	    }
	    if ( (ptr = get_num(ptr, &i, 24, 0, NULL)) == NULL )
		Handle_err;
	    if ( negative )
		cl->cl_file->cf_tzdiff = (- i);
	    else
		cl->cl_file->cf_tzdiff = i;

 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" (-)%d\n", opt_name, i);
	}
	

	/* options related to a line (or a set of lines) */
	else if(strcmp(opt_name, "s") == 0 || strcmp(opt_name, "serial") == 0){
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if (i == 0 )
		clear_serial(cl->cl_option);
	    else
		set_serial(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if (strcmp(opt_name, "serialonce") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if (i == 0 )
		set_serial_sev(cl->cl_option);
	    else
		clear_serial_sev(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if (strcmp(opt_name, "lavgonce") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if (i == 0 )
		set_lavg_sev(cl->cl_option);
	    else
		clear_lavg_sev(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if (strcmp(opt_name, "exesev") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if (i == 0 )
		clear_exe_sev(cl->cl_option);
	    else
		set_exe_sev(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if(strcmp(opt_name, "b")==0 || strcmp(opt_name, "bootrun")==0){
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 0 )
		clear_bootrun(cl->cl_option);
	    else
		set_bootrun(cl->cl_option);	
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if( strcmp(opt_name, "reset")==0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 1 ) {
		bzero(cl, sizeof(CL));
		Set(cl->cl_runas, user);
		Set(cl->cl_mailto, user);
		set_default_opt(cl->cl_option);
	    }
	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}

	else if(strcmp(opt_name, "f") == 0 || strcmp(opt_name, "first") == 0){
	    if( ! in_brackets || (ptr=get_time(ptr, &(cl->cl_nextexe)))==NULL)
		Handle_err;
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %ld\n",opt_name,cl->cl_nextexe);
	}

	else if(strcmp(opt_name, "r")==0 || strcmp(opt_name, "runfreq")==0) {
	    if( !in_brackets || (ptr=get_num(ptr,&i,USHRT_MAX,0,NULL)) == NULL)
		Handle_err;
	    if (i <= 1)
		fprintf(stderr, "runfreq must be 2 or more.\n");
	    if (i <= 1)
		Handle_err;	
	    cl->cl_runfreq = i;
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	/* options to run once per interval :
	 * ignore every fields below the limit */
	else if (strcmp(opt_name, "mins") == 0) {
	    /* nothing to do */
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "hours") == 0) {
	    set_freq_mins(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "days") == 0) {
	    set_freq_mins(cl->cl_option);
	    set_freq_hrs(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "mons") == 0) {
	    set_freq_mins(cl->cl_option);
	    set_freq_hrs(cl->cl_option);
	    set_freq_days(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "dow") == 0) {
	    set_freq_mins(cl->cl_option);
	    set_freq_hrs(cl->cl_option);
	    set_freq_days(cl->cl_option);
	    set_freq_mons(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}

	/* run once an element of the selected field 
	 * (once an hour, once a day, etc) */
	else if (strcmp(opt_name, "hourly") == 0) {
	    set_freq_hrs(cl->cl_option);
	    set_freq_periodically(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "daily") == 0) {
	    set_freq_days(cl->cl_option);
	    set_freq_periodically(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "monthly") == 0) {
	    set_freq_mons(cl->cl_option);
	    set_freq_periodically(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "weekly") == 0) {
	    set_freq_dow(cl->cl_option);
	    set_freq_periodically(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}

	/* run once an element of the selected field 
	 * from middle to middle of that field 
	 * (ie once from 12h to 12h the following day) */
	else if (strcmp(opt_name, "midhourly") == 0) {
	    set_freq_hrs(cl->cl_option);
	    set_freq_periodically(cl->cl_option);
	    set_freq_mid(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "middaily") == 0 
		 || strcmp(opt_name, "nightly") == 0) {
	    set_freq_days(cl->cl_option);
	    set_freq_periodically(cl->cl_option);
	    set_freq_mid(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "midmonthly") == 0) {
	    set_freq_mons(cl->cl_option);
	    set_freq_periodically(cl->cl_option);
	    set_freq_mid(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}
	else if (strcmp(opt_name, "midweekly") == 0) {
	    set_freq_dow(cl->cl_option);
	    set_freq_periodically(cl->cl_option);
	    set_freq_mid(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\"\n", opt_name);
	}

	else if ( strcmp(opt_name, "strict") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if (i == 0 )
		clear_strict(cl->cl_option);
	    else
		set_strict(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if ( strcmp(opt_name, "noticenotrun") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if (i == 0 )
		clear_notice_notrun(cl->cl_option);
	    else
		set_notice_notrun(cl->cl_option);
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if ( strcmp(opt_name, "lavg") == 0 ) {
	    if (!in_brackets || (ptr=get_num(ptr,&i,UCHAR_MAX,1,NULL)) == NULL)
		Handle_err;
	    cl->cl_lavg[0] = i;
 	    if (debug_opt)
		fprintf(stderr, "  Opt : 'lavg1' %d\n", i);
	    if ( *ptr++ != ',' )
		Handle_err;
	    if (!in_brackets || (ptr=get_num(ptr,&i,UCHAR_MAX,1,NULL)) == NULL)
		Handle_err;
	    cl->cl_lavg[1] = i;
 	    if (debug_opt)
		fprintf(stderr, "  Opt : 'lavg5' %d\n", i);
	    if ( *ptr++ != ',' )
		Handle_err;
	    if(!in_brackets || (ptr=get_num(ptr,&i,UCHAR_MAX,1,NULL)) == NULL )
		Handle_err;
	    cl->cl_lavg[2] = i;
	    set_lavg(cl->cl_option);
#ifdef NOLOADAVG
	    warn("As fcron has been compiled with no procfs support,\n"
		 "you will not be able to use the lavg* options");
#endif /* NOLOADAVG */ 
 	    if (debug_opt)
		fprintf(stderr, "  Opt : 'lavg15' %d\n", i);
	}

	else if( strcmp(opt_name, "lavg1") == 0 ) {
	    if(!in_brackets ||(ptr=get_num(ptr, &i, UCHAR_MAX, 1, NULL))==NULL)
		Handle_err;
	    cl->cl_lavg[0] = i;
	    set_lavg(cl->cl_option);
#if NOLOADAVG
	    warn("As fcron has been compiled with no procfs support,\n"
		 "you will not be able to use the lavg* options");
#endif /* NOLOADAVG */ 
 	    if (debug_opt)
		fprintf(stderr, "  Opt : 'lavg1' %d\n", i);
	}

	else if( strcmp(opt_name, "lavg5") == 0 ) {
	    if(!in_brackets ||(ptr=get_num(ptr, &i, UCHAR_MAX, 1, NULL))==NULL)
		Handle_err;
	    cl->cl_lavg[1] = i;
	    set_lavg(cl->cl_option);
#ifdef NOLOADAVG
	    warn("As fcron has been compiled with no procfs support,\n"
		 "you will not be able to use the lavg* options");
#endif /* NOLOADAVG = 0 */ 
 	    if (debug_opt)
		fprintf(stderr, "  Opt : 'lavg5' %d\n", i);
	}

	else if( strcmp(opt_name, "lavg15") == 0 ) {
	    if(!in_brackets ||(ptr=get_num(ptr, &i, UCHAR_MAX, 1, NULL))==NULL)
		Handle_err;
	    cl->cl_lavg[2] = i;
	    set_lavg(cl->cl_option);
#ifdef NOLOADAVG
	    warn("As fcron has been compiled with no procfs support,\n"
		 "you will not be able to use the lavg* options");
#endif /* NOLOADAVG = 0 */ 
 	    if (debug_opt)
		fprintf(stderr, "  Opt : 'lavg15' %d\n", i);
	}

	else if( strcmp(opt_name, "lavgand") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 0 )
		set_lor(cl->cl_option);
	    else
		set_land(cl->cl_option);	
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if( strcmp(opt_name, "lavgor") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 0 )
		set_land(cl->cl_option);
	    else
		set_lor(cl->cl_option);	
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if(strcmp(opt_name, "u") == 0 || strcmp(opt_name, "until") == 0){
	    if( ! in_brackets || (ptr=get_time(ptr, &(cl->cl_until)))==NULL)
		Handle_err;
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %ld\n",opt_name,cl->cl_until);
	}

	else if(strcmp(opt_name, "m")==0 || strcmp(opt_name, "mail")==0){
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 0 )
		clear_mail(cl->cl_option);
	    else
		set_mail(cl->cl_option);	
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if( strcmp(opt_name, "forcemail") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 0 )
		clear_mailzerolength(cl->cl_option);
	    else
		set_mailzerolength(cl->cl_option);	
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if( strcmp(opt_name, "mailto") == 0) {
	    char buf[50];
	    struct passwd *pass = NULL;
	    bzero(buf, sizeof(buf));

	    if( ! in_brackets )
		Handle_err;

	    i = 0;
	    while ( isalnum( (int) *ptr) && i + 1 < sizeof(buf) )
		buf[i++] = *ptr++;
	    if ( strcmp(buf, "\0") == 0 )
		clear_mail(cl->cl_option);
	    else {
		if ( (pass = getpwnam(buf)) == NULL ) {
		    fprintf(stderr, "%s:%d:mailto: \"%s\" is not in passwd :"
			    " ignored\n", file_name, line, buf);	
		    need_correction = 1;
		} else {
		    Set(cl->cl_mailto, buf);
		    set_mail(cl->cl_option);
		}
	    }
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" \"%s\"\n", opt_name, buf);
	}

	else if( strcmp(opt_name, "dayand") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 0 )
		set_dayor(cl->cl_option);
	    else
		set_dayand(cl->cl_option);	
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if( strcmp(opt_name, "dayor") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 0 )
		set_dayand(cl->cl_option);
	    else
		set_dayor(cl->cl_option);	
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if( strcmp(opt_name, "nolog") == 0 ) {
	    if ( in_brackets && (ptr = get_bool(ptr, &i)) == NULL )
		Handle_err;
	    if ( i == 0 )
		clear_nolog(cl->cl_option);
	    else
		set_nolog(cl->cl_option);	
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" %d\n", opt_name, i);
	}

	else if(strcmp(opt_name, "n") == 0 || strcmp(opt_name, "nice") == 0) {
	    if( ! in_brackets || (ptr = get_nice(ptr, &i)) == NULL )
		Handle_err;
	    cl->cl_nice = (char)i;
 	    if (debug_opt)
		fprintf(stderr, "  Opt : \"%s\" (-)%d\n", opt_name, i);
	}

	else if(strcmp(opt_name, "runas") == 0) {
	    if (getuid() != 0) {
		fprintf(stderr, "must be privileged to use option runas: "
			"skipping option\n");
		need_correction = 1;
		if ( ! in_brackets )
		    Handle_err;
		while( *ptr != ')' && *ptr != ' ' && *ptr != '\0' )
		    ptr++;
	    }
	    else {
		char name[USER_NAME_LEN];
		struct passwd *pas;
		int i = 0;

		if( ! in_brackets )
		    Handle_err;

		bzero(name, sizeof(name));

		while ( *ptr != ')' && i < sizeof(name))
		    name[i++] = *ptr++;
    
		if ((pas = getpwnam(name)) == NULL) {
		    fprintf(stderr, "runas: \"%s\" is not in passwd file : "
			    "ignored", name);
		    need_correction = 1;
		} 
		else {
		    Set(cl->cl_runas, name);
		    if (debug_opt)
			fprintf(stderr, "  Opt : \"%s\" %s\n", opt_name, name);
		}

	    }
	}

	else {
	    fprintf(stderr, "%s:%d: Option \"%s\" unknown: "
		    "skipping option.\n", file_name, line, opt_name);  
	    need_correction = 1;
	}
	
	if ( in_brackets ) {
	    if ( *ptr != ')' )
		{ Handle_err }
	    else
		ptr++;
	}

    } while ( *ptr == ',' && ptr++);
	
    Skip_blanks(ptr);
    return ptr;
}


char *
get_time(char *ptr, time_t *time)
    /* convert time read in string in time_t format */
{
    time_t sum;
    
    *time = 0 ;

    while( (*ptr != ' ') && (*ptr != '\t') && (*ptr != '\0') &&
	   (*ptr != ')') ) { 

	sum = 0;

	while ( isdigit( (int) *ptr) ) {
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
	    sum *= 3600;
	    ptr++;
	    break;
	case ' ':
	case '\t':
	case ')':
	    sum *= 60;          /* minutes */
	    break;
	default:
	    need_correction = 1;
	    return NULL;
	}

	*time += sum;

    }

    Skip_blanks(ptr);
    if (*time == 0) {
	need_correction = 1;
	return NULL;
    }
    else
	return ptr;
}


char *
check_username(char *ptr, CF *cf, CL *cl)
    /* check ptr to see if the first word is a username, returns new ptr */
{
    short int indx = 0;
    char username[USER_NAME_LEN];
    struct passwd *userpwent;

    /* check to see if next word is a username */
    while ( isalnum( (int) ptr[indx]) ) indx++;
    if (indx > USER_NAME_LEN) indx = USER_NAME_LEN;
    strncpy(username, ptr, indx);
    username[indx] = '\0';

    if ((userpwent = getpwnam(username)) != NULL) {
	/* found the user */
        ptr = ptr + indx;	/* move ptr to the next word */
	Skip_blanks(ptr);

	if (getuid() != 0) {
	    fprintf(stderr, "must be privileged to run as another user : "
		    "ignoring\n");
	} else {
	    Set(cl->cl_runas, username);
            if (debug_opt)
		fprintf(stderr, "  Opt : inline_runas %s\n", username);
	}
    }

    return ptr;
}


void
read_freq(char *ptr, CF *cf)
    /* read a freq entry, and append a line to cf */
{
    CL *cl = NULL;
    
    Alloc(cl, CL);
    memcpy(cl, &default_line, sizeof(CL));
    cl->cl_runas = strdup2(default_line.cl_runas);
    cl->cl_mailto = strdup2(default_line.cl_mailto);

    /* skip the @ */
    ptr++;

    /* get the time before first execution or the options */
    if ( isdigit( (int) *ptr) ) {
	if ( (ptr = get_time(ptr, &(cl->cl_nextexe))) == NULL ) {
	    fprintf(stderr, "%s:%d: Error while reading first delay:"
		    " skipping line.\n", file_name, line);
	    goto exiterr;
	}
	
	Skip_blanks(ptr);
    }
    else if ( isalnum( (int) *ptr) ) {
	if ( (ptr = read_opt(ptr, cl)) == NULL )
	    goto exiterr;
    }
    else
	Skip_blanks(ptr);

    /* we set this here, because it may be unset by read_opt (reset option) */
    cl->cl_runfreq = 0;
    set_freq(cl->cl_option);

    /* then cl_timefreq */
    if ( (ptr = get_time(ptr, &(cl->cl_timefreq))) == NULL) {
	fprintf(stderr, "%s:%d: Error while reading frequency:"
		" skipping line.\n", file_name, line);
	goto exiterr;
    }
	
    if ( cl->cl_timefreq == 0) {
	fprintf(stderr, "%s:%d: no freq specified: skipping line.\n",
		file_name, line);
	goto exiterr;
    }

    if ( cl->cl_nextexe == 0 )
	/* time before first execution is not specified */
	cl->cl_nextexe = cl->cl_timefreq;

    /* check for inline runas */
    ptr = check_username(ptr, cf, cl);
    
    /* get cl_shell field ( remove trailing blanks ) */
    if ( (cl->cl_shell = get_string(ptr)) == NULL ) {
	fprintf(stderr, "%s:%d: Mismatched quotes: skipping line.\n",
		file_name, line);
	goto exiterr;
  }
    if ( strcmp(cl->cl_shell, "\0") == 0 ) {
	fprintf(stderr, "%s:%d: No shell command: skipping line.\n",
		file_name, line);
	free(cl->cl_shell);
	goto exiterr;
    }

#ifndef SENDMAIL
    clear_mail(cl->cl_option);
#endif

    cl->cl_next = cf->cf_line_base;
    cf->cf_line_base = cl;

    if ( debug_opt )
	fprintf(stderr, "  Cmd \"%s\", timefreq %ld, first %ld\n",
		cl->cl_shell, cl->cl_timefreq, cl->cl_nextexe);
    
    return;

  exiterr:
    free(cl);
    need_correction = 1;
    return;
}



#define R_field(ptr, ary, max, aryconst, descrp) \
  if((ptr = read_field(ptr, ary, max, aryconst)) == NULL) { \
      if (debug_opt) \
          fprintf(stderr, "\n"); \
      fprintf(stderr, "%s:%d: Error while reading " descrp " field: " \
             "skipping line.\n", file_name, line); \
      free(cl); \
      return; \
  }

void
read_arys(char *ptr, CF *cf)
    /* read a run freq number plus a normal fcron line */
{
    CL *cl = NULL;
    unsigned int i = 0;

    Alloc(cl, CL);
    memcpy(cl, &default_line, sizeof(CL));
    cl->cl_runas = strdup2(default_line.cl_runas);
    cl->cl_mailto = strdup2(default_line.cl_mailto);

    /* set cl_remain if not specified */
    if ( *ptr == '&' ) {
	ptr++;
	if ( isdigit( (int) *ptr) ) {
	    if ( (ptr = get_num(ptr, &i, USHRT_MAX, 0, NULL)) == NULL ) {
		fprintf(stderr, "%s:%d: Error while reading runfreq:"
			" skipping line.\n", file_name, line);
		goto exiterr;
	    } else {
		if (i <= 1) {
		    fprintf(stderr, "%s:%d: runfreq must be 2 or more :"
			    " skipping line.\n", file_name, line);
		    goto exiterr;
		}
		cl->cl_runfreq = i;
	    }
	}
	else if ( isalnum( (int) *ptr) )
	    if ( (ptr = read_opt(ptr, cl)) == NULL ) {
		goto exiterr;
	    }
	Skip_blanks(ptr);
    }

    cl->cl_remain = cl->cl_runfreq;

    /* we set this here, because it may be unset by read_opt (reset option) */
    set_td(cl->cl_option);

    if (debug_opt)
	fprintf(stderr, "     ");

    /* get the fields (check for errors) */
    R_field(ptr, cl->cl_mins, 59, NULL, "minutes");
    R_field(ptr, cl->cl_hrs, 23, NULL, "hours");
    R_field(ptr, cl->cl_days, 31, NULL, "days");
    /* month are defined by user from 1 to 12 : max is 12 */
    R_field(ptr, cl->cl_mons, 12, mons_ary, "months");
    R_field(ptr, cl->cl_dow, 7, dows_ary, "days of week");

    if (debug_opt)
	/* if debug_opt is set, we print informations in read_field function,
	 *  but no end line : we print it here */
	fprintf(stderr, " remain %d\n", cl->cl_remain);

    /* check for inline runas */
    ptr = check_username(ptr, cf, cl);
    
    /* get the shell command (remove trailing blanks) */
    if ( (cl->cl_shell = get_string(ptr)) == NULL ) {
	fprintf(stderr, "%s:%d: Mismatched quotes: skipping line.\n",
		file_name, line);
	goto exiterr;	
    }
    if ( strcmp(cl->cl_shell, "\0") == 0 ) {
	fprintf(stderr, "%s:%d: No shell command: skipping line.\n",
		file_name, line);
	goto exiterr;
    }

#ifndef SENDMAIL
    clear_mail(cl->cl_option);
#endif

    cl->cl_next = cf->cf_line_base;
    cf->cf_line_base = cl;

    if ( debug_opt )
	fprintf(stderr, "  Cmd \"%s\"\n", cl->cl_shell);
    return;

  exiterr:
    need_correction = 1;
    free(cl);
    return;

}

void
read_period(char *ptr, CF *cf)
    /* read a line to run periodically (i.e. once a day, once a week, etc) */
{
    CL *cl = NULL;
    short int remain = 8;

    Alloc(cl, CL);
    memcpy(cl, &default_line, sizeof(CL));
    cl->cl_runas = strdup2(default_line.cl_runas);
    cl->cl_mailto = strdup2(default_line.cl_mailto);

    /* skip the % */
    ptr++;

    /* set cl_remain if not specified */
    if ( (ptr = read_opt(ptr, cl)) == NULL ) {
	goto exiterr;
    }
    Skip_blanks(ptr);

    /* a runfreq set to 1 means : this is a periodical line */
    cl->cl_remain = cl->cl_runfreq = 1;

    /* we set this here, because it may be unset by read_opt (reset option) */
    set_td(cl->cl_option);

    if (debug_opt)
	fprintf(stderr, "     ");

    if ( is_freq_periodically(cl->cl_option) ) {
	if (is_freq_mins(cl->cl_option)) remain = 0;
	else if (is_freq_hrs(cl->cl_option)) remain = 1;
	else if (is_freq_days(cl->cl_option)) remain = 2;
	else if (is_freq_mons(cl->cl_option)) remain = 3;
	else if (is_freq_dow(cl->cl_option)) remain = 2;
    }

    /* get the fields (check for errors) */
    if ( remain-- > 0) { R_field(ptr, cl->cl_mins, 59, NULL, "minutes") }
    else bit_nset(cl->cl_mins, 0, 59);
    if ( remain-- > 0) { R_field(ptr, cl->cl_hrs, 23, NULL, "hours") }
    else bit_nset(cl->cl_hrs, 0, 23);    
    if ( remain-- > 0) { R_field(ptr, cl->cl_days, 31, NULL, "days") } 
    else bit_nset(cl->cl_days, 1, 32);
    /* month are defined by user from 1 to 12 : max is 12 */
    if ( remain-- > 0) { R_field(ptr, cl->cl_mons, 12, mons_ary, "months") }
    else bit_nset(cl->cl_mons, 0, 11);
    if ( remain-- > 0) { R_field(ptr, cl->cl_dow,7, dows_ary, "days of week") }
    else bit_nset(cl->cl_dow, 0, 7);

    if (debug_opt)
	/* if debug_opt is set, we print informations in read_field function,
	 *  but no end line : we print it here */
	fprintf(stderr, " remain %d\n", cl->cl_remain);

    /* check for inline runas */
    ptr = check_username(ptr, cf, cl);
    
    /* get the shell command (remove trailing blanks) */
    if ( (cl->cl_shell = get_string(ptr)) == NULL ) {
	fprintf(stderr, "%s:%d: Mismatched quotes: skipping line.\n",
		file_name, line);
	goto exiterr;	
    }
    if ( strcmp(cl->cl_shell, "\0") == 0 ) {
	fprintf(stderr, "%s:%d: No shell command: skipping line.\n",
		file_name, line);
	goto exiterr;
    } 
    else if ( cl->cl_shell[0] == '*' || isdigit( (int) cl->cl_shell[0]) )
	fprintf(stderr, "%s:%d: Warning : shell command beginning by '%c'.\n",
		file_name, line, cl->cl_shell[0]);

    /* check for non matching if option runfreq is set to 1 */
    if ( ! is_freq_periodically(cl->cl_option) ) {
	const size_t s_mins=60, s_hrs=24;
	const size_t s_days=32, s_mons=12;
	const size_t s_dow=8;
	int j = 0;
	
	if ( ! is_freq_mins(cl->cl_option) ) {
	    bit_ffc(cl->cl_mins, s_mins, &j);
	    if ( j != -1 && j < s_mins) goto ok;
	}
	if ( ! is_freq_hrs(cl->cl_option) ) {
	    bit_ffc(cl->cl_hrs, s_hrs, &j);
	    if ( j != -1 && j < s_hrs) goto ok;
	}
	if ( ! is_freq_days(cl->cl_option) ) {
	    bit_ffc(cl->cl_days, s_days, &j);
	    if ( j != -1 && j < s_days) {
		if ( is_dayand(cl->cl_option) )
		    goto ok;
		else {
		    if ( ! is_freq_dow(cl->cl_option) ) {
			bit_ffc(cl->cl_dow, s_dow, &j);
			if ( j != -1 && j < s_dow) goto ok;
		    }
		}
	    }
	}
	if ( ! is_freq_mons(cl->cl_option) ) {
	    bit_ffc(cl->cl_mons, s_mons, &j);
	    if ( j != -1 && j < s_mons ) goto ok;
	}
	if ( ! is_freq_dow(cl->cl_option) ) {
	    bit_ffc(cl->cl_dow, s_dow, &j);
	    if ( j != -1 && j < s_dow && is_dayand(cl->cl_option) ) goto ok;
	}

	fprintf(stderr, "%s:%d: periodical line with no intervals: "
		"skipping line.\n", file_name, line);
	goto exiterr;
    }

  ok:
#ifndef SENDMAIL
    clear_mail(cl->cl_option);
#endif

    cl->cl_next = cf->cf_line_base;
    cf->cf_line_base = cl;

    if ( debug_opt )
	fprintf(stderr, "  Cmd \"%s\"\n", cl->cl_shell);
    return;

  exiterr:
    need_correction = 1;
    free(cl);
    return;

}

char *
get_num(char *ptr, int *num, int max, short int decimal, const char **names)
    /* read a string's number and return it under int format.
     *  Also check if that number is less than max */
{
    int i = 0;
    *num = 0;

    if ( isalpha( (int) *ptr) ) {

	if ( names == NULL ) {
	    need_correction = 1;
	    return NULL;
	}

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
	need_correction = 1;
	return NULL;

    } else {

	while ( isdigit( (int) *ptr) || *ptr == '.') { 

	    if ( *ptr == '.' && ptr++ && i++ > 0 )
		return NULL;
	    if ( i > 0 && --decimal < 0 ) {
		/* the decimal number is exceeded : we round off,
		 * skip the other decimals and return */
		if ( *ptr >= '5' )
		    *num += 1;
		while ( isdigit( (int) *(++ptr)) ) ;
		ptr--;
	    } else {
		*num *= 10; 
		*num += *ptr - 48; 
	    }

	    if (*num > max) {
		need_correction = 1;
		return NULL;
	    }

	    ptr++; 

	} 

	if ( decimal > 0 )
	    *num *= 10 * decimal;

	if ( max == 12 )
	    /* this number is part of the month field.
	     * user set it from 1 to 12, but we manage it internally
	     * as a number from 0 to 11 : we remove 1 to *num */
	    *num = *num - 1;

    }

    return ptr;
}


char *
read_field(char *ptr, bitstr_t *ary, int max, const char **names)
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
	    /* user set month from 1 to 12, but we manage it internally
	     * as a number from 0 to 11 */
	    stop = ( max == 12 ) ? 11 : max;
	    ptr++;
	} else {

	    if ( (ptr = get_num(ptr, &start, max, 0, names)) == NULL )
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
		if ( (ptr = get_num(ptr, &stop, max, 0, names)) == NULL )
		    return NULL;
	    } else {
		/* syntax error */
		need_correction = 1;
		return NULL;
	    }
	}

	/* check for step size */
	if ( *ptr == '/' ) {
	    ptr++;
	    if ((ptr = get_num(ptr, &step, max, 0, names))==NULL || step == 0)
		return NULL;
	} else
	    /* step undefined : default is 0 */
	    step = 1;
	
	/* fill array */	
	if (debug_opt)
	    fprintf(stderr, " %d-%d/%d", start, stop, step);

	if (start < stop)
	    for (i = start;  i <= stop;  i += step)
		bit_set(ary, i);
	else {
	    short int field_max = ( max == 12 ) ? 11 : max;
	    /* this is a field like (for hours field) "22-3" :
	     * we should set from 22 to 3 (not from 3 to 22 or nothing :)) ) */
	    for (i = start;  i <= field_max;  i += step)
		bit_set(ary, i);
	    for (i -= (field_max + 1);  i <= stop;  i += step)
		bit_set(ary, i);
	}

	/* finally, remove unwanted values */
	while ( *ptr == '~' ) {
	    ptr++;
	    rm = 0;
	    if ( (ptr = get_num(ptr, &rm, max, 0, names)) == NULL )
		return NULL;

	    if (debug_opt)
		fprintf(stderr, " ~%d", rm);	    
	    bit_clear(ary, rm);

	    /* if we remove one value of Sunday, remove the other */
	    if (max == 7 && rm == 0) {
		bit_clear(ary, 7);
	    if (debug_opt)
		fprintf(stderr, " ~%d", 7);	    
	    }
	    else if (max == 7 && rm == 7) {
		bit_clear(ary, 0);
	    if (debug_opt)
		fprintf(stderr, " ~%d", 0);	    	    
	    }

	}

    }

    /* Sunday is both 0 and 7 : if one is set, set the other */
    if ( max == 7 ) {
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
		free(line->cl_mailto);
		free(line->cl_runas);
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
	free(env->e_val);
	free(env);
    }

    /* finally free file itself */
    free(file->cf_user);
    free(file);

}


/* save_file() error management */
#define Save_type(file, type) \
        { \
          if ( save_type(file, type) != OK ) { \
            error_e("Could not write type : file has not been installed."); \
            fclose(file); \
            remove(path); \
            return ERR; \
	  } \
        }

#define Save_str(file, type, str) \
        { \
          if ( save_str(file, type, str) != OK ) { \
            error_e("Could not write str : file has not been installed."); \
            fclose(file); \
            remove(path); \
            return ERR; \
	  } \
        }

#define Save_strn(file, type, str, size) \
        { \
          if ( save_strn(file, type, str, size) != OK ) { \
            error_e("Could not write strn : file has not been installed."); \
            fclose(file); \
            remove(path); \
            return ERR; \
	  } \
        }

#define Save_lint(file, type, value) \
        { \
          if ( save_lint(file, type, value) != OK ) { \
            error_e("Could not write lint : file has not been installed."); \
            fclose(file); \
            remove(path); \
            return ERR; \
	  } \
        }

int
save_file(char *path)
    /* Store the informations relatives to the executions
     * of tasks at a defined frequency of system's running time */
{
    CF *file = NULL;
    CL *line = NULL;
    FILE *f = NULL;
    env_t *env = NULL;

    if (debug_opt)
	fprintf(stderr, "Saving ...\n");
    
    for (file = file_base; file; file = file->cf_next) {

	/* open file */
	if ( (f = fopen(path, "w")) == NULL ) {
	    error_e("Could not open %s : file has not be installed.", path);
	    return ERR;
	}

	/* save file : */

	/* put program's version : it permit to daemon not to load
	 * a file which he won't understand the syntax, for exemple
	 * a file using a depreciated format generated by an old fcrontab,
	 * if the syntax has changed */
	/* an binary fcrontab *must* start by such a header */
	Save_lint(f, S_HEADER_T, S_FILEVERSION );

	/* put the user's name : needed to check if his uid has not changed */
	/* S_USER_T *must* be the 2nd field of a binary fcrontab */
 	Save_str(f, S_USER_T, user);

	/* put the time & date of saving : this is use for calcutating 
	 * the system down time. As it is a new file, we set it to 0 */
	/* S_USER_T *must* be the 3rd field of a binary fcrontab */
	Save_lint(f, S_TIMEDATE_T, 0);

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
		/* save the frequency and the first wait time */
		Save_lint(f, S_NEXTEXE_T, line->cl_nextexe);
		Save_lint(f, S_TIMEFREQ_T, line->cl_timefreq);
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
	    {
		Save_type(f, S_ENDLINE_T);
	    }
	}

	fclose(f);

    }
    return OK;
}
