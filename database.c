
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

 /* $Id: database.c,v 1.12 2000-06-20 20:36:26 thib Exp $ */

#include "fcron.h"

int is_leap_year(int year);
int get_nb_mdays(int year, int mon);
void goto_non_matching(CL *line, struct tm *tm);



void
test_jobs(time_t t2)
  /* determine which jobs need to be run, and run them. */
{
    struct job *j;

    debug("Looking for jobs to execute ...");

    while ( (j=queue_base) && j->j_line->cl_nextexe <= t2 ){
	set_next_exe(j->j_line, 0);
	if ( j->j_line->cl_remain > 0 && --(j->j_line->cl_remain) > 0) {
	    debug("    cl_remain: %d", j->j_line->cl_remain);
	    continue ;
	}

	j->j_line->cl_remain = j->j_line->cl_runfreq;

	if (j->j_line->cl_pid > 0) {
	    warn("    process already running: %s '%s'", 
		 j->j_line->cl_file->cf_user,
		 j->j_line->cl_shell
		);
	} 
	else
	    run_job(j->j_line);
    }

}


void
wait_chld(void)
  /* wait_chld() - check for job completion */
{
    short int i = 0;
    int pid;
    CL *line = NULL;


//
    debug("wait_chld");
//
    while ( (pid = wait3(NULL, WNOHANG, NULL)) > 0 ) {
	i = 0;
	while ( i < exe_array_size ) {
	    line = exe_array[i];
	    if (line != NULL && pid == line->cl_pid) {
		exe_array[i]->cl_pid = 0;
		exe_array[i]->cl_file->cf_running -= 1;
		if (i < --exe_num) {
		    exe_array[i] = exe_array[exe_num];
		    exe_array[exe_num] = NULL;
		}
		else
		    exe_array[i] = NULL;

		goto nextloop;
	    }
	    i++;
	}
	/* execution shouldn't come here */
	error("not in exe_array !");
      nextloop:
    }

}


void
wait_all(int *counter)
   /* return after all jobs completion. */
{
    short int i = 0;
    int pid;

    debug("Waiting for all jobs");

    while ( (*counter > 0) && (pid = wait3(NULL, 0, NULL)) > 0 ) {
	i = 0;
	while ( i < exe_array_size ) {
	    if (pid == exe_array[i]->cl_pid) {
		exe_array[i]->cl_pid = 0;
		exe_array[i]->cl_file->cf_running -= 1;
		if (i < --exe_num) {
		    exe_array[i] = exe_array[exe_num];
		    exe_array[exe_num] = NULL;
		}
		else
		    exe_array[i] = NULL;

		goto nextloop;
	    }
	    i++;
	}
	/* execution shouldn't come here */
	error("not in exe_array !");
      nextloop:
    }    

}


int
is_leap_year(int year)
  /* return 1 if it's a leap year otherwise return 0 */
{
    return ( (year % 4 == 0) &&
	     ( (year % 100 != 0) || (year % 400 == 0) ) );

}


int
get_nb_mdays(int year, int mon)
  /* return the number of days in a given month of a given year */
{
    if (mon == 1) { /* is February ? */
	if ( is_leap_year(year) )
	    return 29;
	else
	    return 28;
    }
    else if (mon <= 6)
	if (mon % 2 == 0)
	    return 31;
	else
	    return 30;
    else if (mon % 2 == 0)
	return 30;
    else 
	return 31;

}


void
set_wday(struct tm *date)
  /* we know that 01/01/2000 was a Saturday ( day number 6 )
   * so we count the number of days since 01/01/2000,
   * and add this number modulo 7 to the wday number */
{
    long nod = 0;
    register int i;

    /* we add the number of days of each previous years */
    for (i =  (date->tm_year - 1); i >= 100 ; i--)
	nod += ( is_leap_year(i+1900) ) ? 366 : 365;

    /*     and month */
    for (i =  (date->tm_mon - 1); i >= 0; i--)
	nod += get_nb_mdays( (date->tm_year + 1900), i);

    /* then we add the number of days passed in the current month */
    nod += (date->tm_mday - 1); /* (mday is set from 1 to 31) */

    date->tm_wday = (nod % 7) + 6;

    if ( date->tm_wday >= 7 )
	date->tm_wday -= 7;

    debug("   dow of %d-%d-%d : %d", (date->tm_mon + 1), date->tm_mday, 
	  ( date->tm_year + 1900 ), date->tm_wday); 

} 


void
goto_non_matching(CL *line, struct tm *ftime)
    /* search the first the nearest time and date that does
     * not match the line */
{
    register int i;
    
    /* check if we are in an interval of execution */
    if( ! ( bit_test(line->cl_mins, ftime->tm_min) &&
	    bit_test(line->cl_hrs, ftime->tm_hour) &&
	    bit_test(line->cl_days, ftime->tm_mday) &&
	    bit_test(line->cl_dow, ftime->tm_wday) &&
	    bit_test(line->cl_mons, ftime->tm_mon)
	) )
	return;
    

    for(i=ftime->tm_min; bit_test(line->cl_mins, i) && (i<60); i++);
    if (i == 60) {
	ftime->tm_min = 0;
	if (ftime->tm_hour++ == 24) {
	    ftime->tm_hour = 0;
	    if(ftime->tm_mday++ == 
	       get_nb_mdays((ftime->tm_year+1900),ftime->tm_mon)) {
		ftime->tm_mday = 0;
		if(ftime->tm_mon++ == 12) {
		    ftime->tm_mon = 0;
		    ftime->tm_year++;
		}
	    }
	}

	set_wday(ftime);

	if( ! ( bit_test(line->cl_hrs, ftime->tm_hour) &&
		bit_test(line->cl_days, ftime->tm_mday) &&
		bit_test(line->cl_dow, ftime->tm_wday) &&
		bit_test(line->cl_mons, ftime->tm_mon)
	    ) )
	    goto exit_fct;

    }
	  
  exit_fct:
    debug("   '%s' first non matching %d/%d/%d wday:%d %02d:%02d",
	  line->cl_shell, (ftime->tm_mon + 1), ftime->tm_mday,
	  (ftime->tm_year + 1900), ftime->tm_wday,
	  ftime->tm_hour, ftime->tm_min);
    return;

}


void 
set_next_exe(CL *line, char is_new_line)
  /* set the cl_nextexe of a given CL and insert it in the queue */
{

    if ( is_td(line->cl_option) ) {

	struct tm *ft;
	struct tm ftime;
	register int i;
	int max;

	ft = localtime(&now);

	/* localtime() function seem to return every time the same pointer :
	   it resets our previous changes, so we need to prevent it
	   ( localtime() is used in the debug() function) */
	memcpy(&ftime, ft, sizeof(struct tm));

	ftime.tm_sec = 0;
	ftime.tm_isdst = -1;

	if ( line->cl_runfreq == 1 && ! is_new_line )
	    goto_non_matching(line, &ftime);
	else
	    /* to prevent multiple execution in the same minute */
	    ftime.tm_min += 1;

    
      setMonth:
	for (i = ftime.tm_mon; (bit_test(line->cl_mons, i)==0) && (i<12); i++);
	if (i >= 12) {
	    ftime.tm_year++;
	    ftime.tm_mon = 0;
	    ftime.tm_mday = 1;
	    ftime.tm_hour = 0;
	    ftime.tm_min = 0;
	    goto setMonth;
	}
	if (ftime.tm_mon !=  i) {
	    ftime.tm_mon = i;
	    ftime.tm_mday = 1;
	    ftime.tm_hour = 0;
	    ftime.tm_min = 0;
	}	    

	/* set the number of days in that month */
	max = get_nb_mdays( (ftime.tm_year + 1900), ftime.tm_mon);

      setDay:
	for (i=ftime.tm_mday; (bit_test(line->cl_days, i)==0)&&(i<=max); i++);
	if (i > max) {
	    ftime.tm_mon++;
	    ftime.tm_mday = 1;
	    ftime.tm_hour = 0;
	    ftime.tm_min = 0;
	    goto setMonth;
	}
	if ( ftime.tm_mday != i ) {
	    ftime.tm_mday = i;
	    ftime.tm_hour = 0;
	    ftime.tm_min = 0;	    
	}

	set_wday(&ftime);

        /* check if the day of week is OK */
	if ( bit_test(line->cl_dow, ftime.tm_wday) == 0 ) {
	    ftime.tm_mday++;
	    ftime.tm_hour = 0;
	    ftime.tm_min = 0;
	    goto setDay;
	}
   
      setHour:
	for (i=ftime.tm_hour; (bit_test(line->cl_hrs, i)==0) && (i<24); i++);
	if (i >= 24) {
	    ftime.tm_mday++;
	    ftime.tm_hour = 0;
	    ftime.tm_min = 0;
	    goto setDay;
	}
	if ( ftime.tm_hour != i ) {
	    ftime.tm_hour = i;
	    ftime.tm_min = 0;
	}
   

	for (i=ftime.tm_min; (bit_test(line->cl_mins, i)==0) && (i<60); i++);
	if (i >= 60) {
	    ftime.tm_hour++;
	    ftime.tm_min = 0;
	    goto setHour;
	}
	ftime.tm_min = i;
   
    
	line->cl_nextexe = mktime(&ftime);

	if ( ! is_new_line )
	    debug("   cmd: '%s' next exec %d/%d/%d wday:%d %02d:%02d",
		  line->cl_shell, (ftime.tm_mon + 1), ftime.tm_mday,
		  (ftime.tm_year + 1900), ftime.tm_wday,
		  ftime.tm_hour, ftime.tm_min);
	
    }
    else {
	/* this is a job based on system up time */
	line->cl_nextexe = now + line->cl_timefreq;
	debug("   cmd: '%s' next exec in %lds", line->cl_shell,
	      line->cl_timefreq);
    }
    

    insert_nextexe(line);

	

}

void
insert_nextexe(CL *line)
    /* insert a job the queue list */
{
    struct job *newjob;

    if (queue_base != NULL) {
	struct job *j;
	struct job *jprev = NULL;
	struct job *old_entry = NULL;

	/* find the job in the list */
	for (j = queue_base; j != NULL ; j = j->j_next)
	    if ( j->j_line == line ) {
		old_entry = j;
		/* remove it from the list */
		if (jprev != NULL) {
		    jprev->j_next = j->j_next;
		    j = jprev;
		}
		else
		    /* first element of the list */
		    j = queue_base = j->j_next;

		break;
	    }

	if (j == NULL || line->cl_nextexe < j->j_line->cl_nextexe)
	    j = queue_base;
	while (j != NULL && (line->cl_nextexe >= j->j_line->cl_nextexe)) {
	    jprev = j;
	    j = j->j_next;
	}	    

	if (old_entry == NULL) {
	    /* this job wasn't in the queue : we append it */
	    Alloc(newjob, job);
	    newjob->j_line = line;
	}
	else
	    /* this job was already in the queue : we move it */
	    newjob = old_entry;

	newjob->j_next = j;

	if (jprev == NULL)
	    queue_base = newjob;
	else
	    jprev->j_next = newjob;

    }
    else {
	/* no job in queue */
	Alloc(newjob, job);
	newjob->j_line = line;	    
	queue_base = newjob;
    }

}

long
time_to_sleep(time_t lim)
  /* return the time to sleep until next task have to be executed. */
{
    /* we set tts to a big value, unless some problems can occurs
     * with files without any line */
    time_t tts = lim;

    if ( queue_base != NULL ) {
	if ( queue_base->j_line->cl_nextexe < lim )
	    tts = queue_base->j_line->cl_nextexe;
    }
	    
    if ( (tts = tts - now) < 0)
	tts = 0;

    debug("Time to sleep: %lds", tts);

    return tts;
}

