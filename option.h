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

 /* $Id: option.h,v 1.4 2000-06-21 13:46:55 thib Exp $ */

/* read and set options of a line */

/*
  
  The options are :
  
  bit:   option:
  0      is this job based on time and date or system up time ?
  1      is this job based on system load average ?
  2      perform a logic OR or a logic AND between load averages ?
  3      should we run this job if it should have been executed
          during system down?
  4      should this job be run serially ?
  5      does the output have to be mailed to user ?
  6      does the output (even if zero-length) must be mailed to user ?

*/

#ifndef __OPTIONH__
#define __OPTIONH__

/* internal macros */

#define	_bit_set(opt, bit) \
	(opt |= (1u << bit))

#define	_bit_test(opt, bit) \
	(opt & (1u << bit))

#define	_bit_clear(opt, bit) \
	(opt &= ~(1u << bit))


/* external macros */

/*
  bit 0 : set to 1 : line is based on system up time
          set to 0 : line is based on time and date
*/
#define	is_freq(opt) \
	(_bit_test(opt, 0))
#define is_td(opt) \
        ( ! _bit_test(opt, 0))
#define	set_freq(opt) \
	(_bit_set(opt, 0))
#define set_td(opt) \
	(_bit_clear(opt, 0))


/*
  bit 1 : set to 1 : line based on system load average
          set to 0 : line doesn't take care of load average
*/
#define	is_lavg(opt) \
	(_bit_test(opt, 1))
#define	set_lavg(opt) \
	(_bit_set(opt, 1))
#define clear_lavg(opt) \
	(_bit_clear(opt, 1))


/*
  bit 2 : set to 1 : perform a logic AND between load averages
          set to 0 : perform a logic OR between load averages
*/
#define	is_land(opt) \
	(_bit_test(opt, 2))
#define	is_lor(opt) \
	( ! _bit_test(opt, 2))
#define	set_land(opt) \
	(_bit_set(opt, 1))
#define	set_lor(opt) \
	(_bit_clear(opt, 2))

/*
  bit 3 : set to 1 : run this line if it should have been executed
                     during system down
          set to 0 : do not run it
*/
#define	is_bootrun(opt) \
	(_bit_test(opt, 3))
#define	set_bootrun(opt) \
	(_bit_set(opt, 3))
#define clear_bootrun(opt) \
	(_bit_clear(opt, 3))


/*
  bit 4 : set to 1 : run this line serially
          set to 0 : do not run it serially
*/
#define	is_serial(opt) \
	(_bit_test(opt, 4))
#define	set_serial(opt) \
	(_bit_set(opt, 4))
#define clear_serial(opt) \
	(_bit_clear(opt, 4))


/*
  bit 5 : set to 1 : do not mail output
          set to 0 : mail output to user
*/
#define	is_mail(opt) \
	( ! _bit_test(opt, 5))
#define	set_mail(opt) \
	(_bit_clear(opt, 5))
#define clear_mail(opt) \
	(_bit_set(opt, 5))


/*
  bit 6 : set to 1 : mail output even if it is zero-length to user
          set to 0 : mail output only if it is non-zero length
*/
#define	is_zerolength(opt) \
	(_bit_test(opt, 6))
#define	set_zerolength(opt) \
	(_bit_set(opt, 6))
#define clear_zerolength(opt) \
	(_bit_clear(opt, 6))


/*
  bit 7 : set to 1 : job is being serialized once
          set to 0 : job is not being serialized once
*/
#define	is_serial_once(opt) \
	(_bit_test(opt, 7))
#define	set_serial_once(opt) \
	(_bit_set(opt, 7))
#define clear_serial_once(opt) \
	(_bit_clear(opt, 7))


#endif /* __OPTIONH__ */

