
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

 /* $Id: database.c,v 1.35 2000-11-02 10:58:03 thib Exp $ */

#include "fcron.h"

int is_leap_year(int year);
int get_nb_mdays(int year, int mon);
void goto_non_matching(CL *line, struct tm *tm);
void run_normal_job(CL *line);
void run_serial_job(void);
void run_lavg_job(int i);
void run_queue_job(CL *line);


void
test_jobs(time_t t2)
  /* determine which jobs need to be run, and run them. */
{
    struct job *j;

/*      // */
    debug("Looking for jobs to execute ...");
/*      // */

    while ( (j=queue_base) && j->j_line->cl_nextexe <= t2 ){
	set_next_exe(j->j_line, 0);
	if ( j->j_line->cl_remain > 0 && --(j->j_line->cl_remain) > 0) {
	    debug("    cl_remain: %d", j->j_line->cl_remain);
	    continue ;
	}

	j->j_line->cl_remain = j->j_line->cl_runfreq;

	if ( is_lavg(j->j_line->cl_option) )
	    add_lavg_job(j->j_line);
	else if ( is_serial(j->j_line->cl_option) )
	    add_serial_job(j->j_line);
	else
	    run_normal_job(j->j_line);
    }

}


void
run_normal_job(CL *line)
{

    if (line->cl_numexe <= 0 || 
	(is_exe_sev(line->cl_option) && line->cl_numexe < UCHAR_MAX)) {
	line->cl_numexe += 1;
	run_queue_job(line);
    }
    else {
	warn("    process already running: %s '%s'", 
	     line->cl_file->cf_user,
	     line->cl_shell
	    );
    } 

}

void
run_lavg_job(int i)
{

    run_queue_job(lavg_array[i].l_line);

    if ( is_serial(lavg_array[i].l_line->cl_option) )
	lavg_serial_running++;

    if (i < --lavg_num) {
	lavg_array[i] = lavg_array[lavg_num];
	lavg_array[lavg_num].l_line = NULL;
    }
    else
	lavg_array[i].l_line = NULL;	

}


void
run_serial_job(void)
    /* run the next serialized job */
{
/*      // */
/*      debug("running next serial job"); */
/*      //     */

    debug("num: %d running:%d  index:%d", serial_num, serial_running,
	  serial_array_index);
    if ( serial_num != 0 ) {
	run_queue_job(serial_array[serial_array_index]);
	serial_array[serial_array_index] = NULL;

	serial_running++;
	if ( ++serial_array_index >= serial_array_size )
	    serial_array_index -= serial_array_size;
	serial_num--;
	
    }
}
		    

void
run_queue_job(CL *line)
    /* run a job */
{

/*      // */
/*      debug("run_queue_job"); */
/*      // */

    /* append job to the list of executed job */
    if ( exe_num >= exe_array_size ) {
	struct exe *ptr = NULL;
	short int old_size = exe_array_size;

	debug("Resizing exe_array");
	exe_array_size = (exe_array_size + EXE_GROW_SIZE);
	
	if ( (ptr = calloc(exe_array_size, sizeof(struct exe))) == NULL )
	    die_e("could not calloc exe_array");

	memcpy(ptr, exe_array, (sizeof(struct exe) * old_size));
	free(exe_array);
	exe_array = ptr;
    }
    exe_array[exe_num].e_line = line;

    run_job(&exe_array[exe_num++]);

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
	    else
		jprev = j;

	jprev = NULL;
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

void
add_serial_job(CL *line)
    /* add the next queued job in serial queue */
{
    short int i;

    /* check if the line is already in the serial queue */
    if ( (is_serial_once(line->cl_option) && line->cl_numexe > 0) ||
	 line->cl_numexe >= UCHAR_MAX ) {
	debug("already in serial queue %s", line->cl_shell);
	return;
    }

/*  	// */
    debug("inserting in serial queue %s", line->cl_shell);
/*  	// */

    if ( serial_num >= serial_array_size ) {
	if ( serial_num >= SERIAL_QUEUE_MAX )
	    /* run next job in the queue before adding the new one */
	    run_serial_job();
	else {
	    CL **ptr = NULL;
	    short int old_size = serial_array_size;

	    debug("Resizing serial_array");
	    serial_array_size = (serial_array_size + SERIAL_GROW_SIZE);
	
	    if ( (ptr = calloc(serial_array_size, sizeof(CL *))) == NULL )
		die_e("could not calloc serial_array");

	    /* copy lines in order to have the first line at the index 0 */
	    memcpy(ptr + serial_array_index, serial_array,
		   (sizeof(CL*) * (old_size - serial_array_index)) );
	    memcpy(ptr, serial_array + (old_size - serial_array_index),
		   (sizeof(CL*) * serial_array_index));
	    serial_array_index = 0;
	    free(serial_array);
	    serial_array = ptr;
	}
    }

    if ( (i = serial_array_index + serial_num) >= serial_array_size )
	i -= serial_array_size;
	
    serial_array[i] = line;

    serial_num++;
    line->cl_numexe += 1;

    debug("num: %d size:%d index:%d curline:%d running:%d", serial_num,
	  serial_array_size, serial_array_index, i, serial_running);


}


void
add_lavg_job(CL *line)
    /* add the next queued job in lavg queue */
{

#if LAVG_ONCE
    /* check if the line is already in the lavg queue */
    if ( line->cl_numexe <= 0) {
#else
    if ( line->cl_numexe < UCHAR_MAX ) {
#endif /* LAVG_ONCE */

/*  	// */
	debug("inserting in lavg queue %s", line->cl_shell);
/*  	// */

	/* append job to the list of lavg job */
	if ( lavg_num >= lavg_array_size ) {
	    if ( lavg_num >= LAVG_QUEUE_MAX ) {
		/* run the next lavg job (the oldest one) */
		register int i;
		int j = 0;
		
		for (i = 1; i < lavg_num; i++)
		    if ( lavg_array[i].l_until < lavg_array[j].l_until )
			j = i;
		run_lavg_job(j);
	    }
	    else {
		struct lavg *ptr = NULL;
		short int old_size = lavg_array_size;
		
		debug("Resizing lavg_array");
		lavg_array_size = (lavg_array_size + LAVG_GROW_SIZE);
		
		if ( (ptr = calloc(lavg_array_size, sizeof(lavg))) == NULL )
		    die_e("could not calloc lavg_array");
		
		memcpy(ptr, lavg_array, (sizeof(lavg) * old_size));
		free(lavg_array);
		lavg_array = ptr;
	    }
	}

	lavg_array[lavg_num].l_line = line;
	line->cl_numexe += 1;
	lavg_array[lavg_num++].l_until = 
	    (line->cl_until > 0) ? now + line->cl_until : 0;

    }
    else
	debug("already in lavg queue %s", line->cl_shell);

}


void
wait_chld(void)
  /* wait_chld() - check for job completion */
{
    short int i = 0;
    int pid;
    CL *line = NULL;


/*      // */
/*      debug("wait_chld"); */
/*      // */

    while ( (pid = wait3(NULL, WNOHANG, NULL)) > 0 ) {
	i = 0;
	while ( i < exe_num ) {
	    if (pid == exe_array[i].e_pid) {
		if ( exe_array[i].e_line == NULL ) {
		    /* the corresponding file has been removed from memory */
		    debug("job finished: pid %d", pid);
		}
		else {
		    
		    line = exe_array[i].e_line;
		    debug("job finished: %s", line->cl_shell);
		    line->cl_numexe -= 1;
		    line->cl_file->cf_running -= 1;
		    
		    if ( is_serial_once(line->cl_option) ) {
			clear_serial_once(line->cl_option);
			if ( --serial_running <= 0 )
			    run_serial_job();
		    }
		    else if ( is_serial(line->cl_option)
			      && ! is_lavg(line->cl_option) ) {
			if (--serial_running <= 0)
			    run_serial_job();
		    }
		    else if ( is_lavg(line->cl_option) && 
			      is_serial(line->cl_option) )
			lavg_serial_running--;
		}
		if (i < --exe_num) {
		    exe_array[i] = exe_array[exe_num];
		    exe_array[exe_num].e_line = NULL;
		}
		else
		    exe_array[i].e_line = NULL;
		
		goto nextloop;
	    }
	    i++;
	}
	/* execution shouldn't come here */
	debug("not in exe_array !");
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
	while ( i < exe_num ) {
	    if (pid == exe_array[i].e_pid) {
		if ( exe_array[i].e_line == NULL ) {
		    /* the corresponding file has been removed from memory */
		    debug("job finished: pid %d", pid);
		}
		else {
		    
		    debug("job finished: %s", exe_array[i].e_line->cl_shell);
		    exe_array[i].e_line->cl_numexe -= 1;
		    exe_array[i].e_line->cl_file->cf_running -= 1;
		    
		    if ( is_serial_once(exe_array[i].e_line->cl_option) )
			clear_serial_once(exe_array[i].e_line->cl_option);
		    
		}
		if (i < --exe_num) {
		    exe_array[i] = exe_array[exe_num];
		    exe_array[exe_num].e_line = NULL;
		}
		else
		    exe_array[i].e_line = NULL;
		
		goto nextloop;
	    }
	    i++;
	}
	/* execution shouldn't come here */
	debug("not in exe_array !");
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
	    (
		(is_dayand(line->cl_option) &&
		 bit_test(line->cl_days, ftime->tm_mday) &&
		 bit_test(line->cl_dow, ftime->tm_wday))
		||
		(is_dayor(line->cl_option) && (
		    bit_test(line->cl_days, ftime->tm_mday) ||
		    bit_test(line->cl_dow, ftime->tm_wday)))
		) &&
	    bit_test(line->cl_mons, ftime->tm_mon)
	) )
	return;
    

  start:
    for(i=ftime->tm_min; bit_test(line->cl_mins, i) && (i<60); i++);
    ftime->tm_min = i;
    if (i == 60) {
	ftime->tm_min = 0;
	if (++ftime->tm_hour == 24) {
	    ftime->tm_hour = 0;
	    set_wday(ftime);
	    if(++ftime->tm_mday == 
	       get_nb_mdays((ftime->tm_year+1900),ftime->tm_mon)) {
		ftime->tm_mday = 0;
		if(++ftime->tm_mon == 12) {
		    ftime->tm_mon = 0;
		    ftime->tm_year++;
		}
	    }
	}

	if( ! ( bit_test(line->cl_mins, ftime->tm_min) &&
		bit_test(line->cl_hrs, ftime->tm_hour) &&
		(
		    (is_dayand(line->cl_option) &&
		     bit_test(line->cl_days, ftime->tm_mday) &&
		     bit_test(line->cl_dow, ftime->tm_wday))
		    ||
		    (is_dayor(line->cl_option) && (
			bit_test(line->cl_days, ftime->tm_mday) ||
			bit_test(line->cl_dow, ftime->tm_wday)))
		    ) &&
		bit_test(line->cl_mons, ftime->tm_mon)
	    ) )
	    goto exit_fct;

	goto start;
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
	register char has_changed = 0;

	ft = localtime(&now);

	/* localtime() function seem to return every time the same pointer :
	   it resets our previous changes, so we need to prevent it
	   ( localtime() is used in the debug() function) */
	memcpy(&ftime, ft, sizeof(struct tm));

	ftime.tm_isdst = -1;

	/* to prevent multiple execution in the same minute */
	ftime.tm_min += 1;
	ftime.tm_sec = 0;
	if ( line->cl_runfreq == 1 && ! is_new_line )
	    goto_non_matching(line, &ftime);

      setMonth:
	for (i = ftime.tm_mon; (bit_test(line->cl_mons, i)==0) && (i<12); i++);
	if (i >= 12) {
	    ftime.tm_year++;
	    if ( has_changed < 3) {
		has_changed = 3;
		ftime.tm_mon = 0;
		ftime.tm_mday = 1;
		ftime.tm_hour = 0;
		ftime.tm_min = 0;
	    }
	    goto setMonth;
	}
	if (ftime.tm_mon !=  i) {
	    ftime.tm_mon = i;
	    if ( has_changed < 2) {
		has_changed = 2;
		ftime.tm_mday = 1;
		ftime.tm_hour = 0;
		ftime.tm_min = 0;
	    }
	}	    

	/* set the number of days in that month */
	max = get_nb_mdays( (ftime.tm_year + 1900), ftime.tm_mon);

      setDay:
	if ( is_dayand(line->cl_option) ) {
	    for (i = ftime.tm_mday; 
		 (bit_test(line->cl_days, i) == 0) && (i <= max); i++);
	    if (i > max) {
		ftime.tm_mon++;
		if ( has_changed < 2) {
		    has_changed = 2;
		    ftime.tm_mday = 1;
		    ftime.tm_hour = 0;
		    ftime.tm_min = 0;
		}
		goto setMonth;
	    }
	    if ( ftime.tm_mday != i ) {
		ftime.tm_mday = i;
		if ( has_changed < 1) {
		    has_changed = 1;
		ftime.tm_hour = 0;
		ftime.tm_min = 0;	
		}    
	    }

	    set_wday(&ftime);

	    /* check if the day of week is OK */
	    if ( bit_test(line->cl_dow, ftime.tm_wday) == 0 ) {
		ftime.tm_mday++;
		ftime.tm_hour = 0;
		ftime.tm_min = 0;
		goto setDay;
	    }
	} else {  /* dayor */
	    register int j;

	    set_wday(&ftime);

	    j = ftime.tm_wday;
	    i = ftime.tm_mday;
	    while( (bit_test(line->cl_days, i) == 0)  &&
		   (bit_test(line->cl_dow, j) == 0) ) {
		if (i > max) {
		    ftime.tm_mon++;
		    if ( has_changed < 2) {
			has_changed = 2;
			ftime.tm_mday = 1;
			ftime.tm_hour = 0;
			ftime.tm_min = 0;
		    }
		    goto setMonth;
		}
		if (j >= 7)
		    j -= 7;
		i++;
		j++;
	    }
	    if ( ftime.tm_mday != i ) {
		ftime.tm_mday = i;
		if ( has_changed < 1) {
		    has_changed = 1;
		    ftime.tm_hour = 0;
		    ftime.tm_min = 0;
		}    
	    }
	}

      setHour:
	for (i=ftime.tm_hour; (bit_test(line->cl_hrs, i)==0) && (i<24); i++);
	if (i >= 24) {
	    ftime.tm_mday++;
	    if ( has_changed < 1) {
		has_changed = 1;
		ftime.tm_hour = 0;
		ftime.tm_min = 0;
	    }
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


time_t
check_lavg(time_t lim)
    /* run a job based on system load average if one should be run
     * and return the time to sleep */
{
    time_t tts = time_to_sleep(lim);

#ifdef NOLOADAVG
    while ( lavg_num > 0 )
	run_lavg_job(0);
    return tts;
#else

    register int i = 0;
    double l_avg[3];

    l_avg[0] = 0;
    l_avg[1] = 0;
    l_avg[2] = 0;

    /* first, check if some lines must be executed because of until */
    while ( i < lavg_num )
	if ( lavg_array[i].l_line->cl_until && lavg_array[i].l_until < now ) {
	    debug("until '%s' %d", lavg_array[i].l_line->cl_shell,
		  lavg_array[i].l_until);
	    run_lavg_job(i);
	} else
	    i++;

    if ( lavg_num == 0 )
	return tts;
	
    if ( (i = getloadavg(l_avg, 3)) != 3 )
	debug("got only %d lavg values", i);
    debug("get_lavg: %lf, %lf, %lf", l_avg[0], l_avg[1], l_avg[2]);
    /* the 3 values stored in the fcron lines are the real value *= 10 */
    l_avg[0] *= 10;
    l_avg[1] *= 10;
    l_avg[2] *= 10;
    i = 0;
    while ( i < lavg_num ) {
	/* check if the line should be executed */
	if ( lavg_serial_running > 0 && 
	     is_serial(lavg_array[i].l_line->cl_option) ) {
	    i++;
	    continue;
	}
	if ( ( is_land(lavg_array[i].l_line->cl_option)
	       && ( l_avg[0] < lavg_array[i].l_line->cl_lavg[0]
		    || lavg_array[i].l_line->cl_lavg[0] == 0 )
	       && ( l_avg[1] < lavg_array[i].l_line->cl_lavg[1]
		    || lavg_array[i].l_line->cl_lavg[1] == 0 )
	       && ( l_avg[2] < lavg_array[i].l_line->cl_lavg[2]
		    || lavg_array[i].l_line->cl_lavg[2] == 0 ) 
	    )
	     || 
	     ( is_lor(lavg_array[i].l_line->cl_option) 
	       &&  ( l_avg[0] < lavg_array[i].l_line->cl_lavg[0]
		     || l_avg[1] < lavg_array[i].l_line->cl_lavg[1]
		     || l_avg[2] < lavg_array[i].l_line->cl_lavg[2] )
		 )
	    ) {
	    debug("lavg '%s' %s %.0f:%d %.0f:%d %.0f:%d",
		  lavg_array[i].l_line->cl_shell,
		  (is_lor(lavg_array[i].l_line->cl_option)) ? "or" : "and",
		  l_avg[0], lavg_array[i].l_line->cl_lavg[0],
		  l_avg[1], lavg_array[i].l_line->cl_lavg[1],
		  l_avg[2], lavg_array[i].l_line->cl_lavg[2]);
	    run_lavg_job(i);

	} else
	    i++;
    }

    
    if ( lavg_num == 0 )
	return tts;
    else
	return (LAVG_SLEEP < tts) ? LAVG_SLEEP : tts;
    
#endif /* def NOLOADAVG */ 

}

time_t
time_to_sleep(time_t lim)
  /* return the time to sleep until next task have to be executed. */
{
    /* we set tts to a big value, unless some problems can occurs
     * with files without any line */
    time_t tts = lim;
    time_t ti = time(NULL);

    if ( queue_base != NULL ) {
	if ( queue_base->j_line->cl_nextexe < lim )
	    tts = queue_base->j_line->cl_nextexe;
    }
	    
    if ( (tts = tts - ti) < 0)
	tts = 0;

/*      // debug("Time to sleep: %lds", tts); */

    return tts;
}

