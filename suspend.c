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

/* code to handle system suspend/hibernate and resume */

#include "suspend.h"
#include "global.h"
#include "fcronconf.h"
#include "fcron.h"
#include "database.h"
#ifdef HAVE_SYS_TIMERFD_H
#include <sys/select.h>
#include <sys/timerfd.h>
#include <stdint.h>             /* for uint64_t */
#ifdef __linux__
/* Not too sure how to get that from the Linux header, so we redefine here instead */
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif                          /* TFD_TIMER_CANCEL_ON_SET */
#endif                          /* __linux__ */
#endif                          /* HAVE_SYS_TIMERFD_H */

/* types */
#if defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC)
struct linux_timeref {
    struct timespec monotonic;
    struct timespec boottime;
};
#endif                          /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */

/* variables */
#if defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC)
struct linux_timeref linux_suspend_ref;
#endif                          /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */
#ifdef TFD_TIMER_CANCEL_ON_SET
int timer_cancel_on_set_fd = -1;
#endif                          /* TFD_TIMER_CANCEL_ON_SET */


/* internal functions */
long int read_suspend_duration(time_t slept_from);
double timespec_diff(struct timespec *from, struct timespec *to);
#if defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC)
void linux_init_timeref(struct linux_timeref *tr);
double linux_get_suspend_time(struct linux_timeref *tr);
#endif                          /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */

double
timespec_diff(struct timespec *from, struct timespec *to)
{
    return ((to->tv_sec - from->tv_sec) * 1000000000.0 +
            (to->tv_nsec - from->tv_nsec)) / 1000000000.0;
}

#if defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC)
void
linux_init_timeref(struct linux_timeref *tr)
    /* Initialize the time reference */
{
    clock_gettime(CLOCK_MONOTONIC, &tr->monotonic);
    clock_gettime(CLOCK_BOOTTIME, &tr->boottime);
}

double
linux_get_suspend_time(struct linux_timeref *tr)
    /* return the suspend time, using Linux-specific APIs */
{
    struct timespec tpm, tpb;
    double tpm_diff, tpb_diff;

    debug("Calculating suspend duration from CLOCK_BOOTTIME-CLOCK_MONOTONIC");

    clock_gettime(CLOCK_MONOTONIC, &tpm);
    clock_gettime(CLOCK_BOOTTIME, &tpb);

    tpm_diff = timespec_diff(&tr->monotonic, &tpm);
    tpb_diff = timespec_diff(&tr->boottime, &tpb);

    linux_init_timeref(tr);

    return (tpb_diff - tpm_diff);
}
#endif                          /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */

long int
read_suspend_duration(time_t slept_from)
  /* Return the amount of time the system was suspended (to mem or disk),
   * as read from suspendfile.
   * Return 0 on error.
   *
   * The idea is that:
   * 1) the OS sends the STOP signal to the main fcron process when suspending
   * 2) the OS writes the suspend duration (as a string) into suspendfile,
   *    and then sends the CONT signal to the main fcron process when resuming.
   *
   * The main reason to do it this way instead of killing fcron and restarting
   * it on resume is to better handle jobs that may already be running.
   * (e.g. don't run them again when the machine resumes) */
{
    int fd = -1;
    char buf[TERM_LEN];
    int read_len = 0;
    long int suspend_duration = 0;      /* default value to return on error */
    struct stat s;

    debug("Attempting to read suspend duration from %s", suspendfile);
    fd = open(suspendfile, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        /* If the file doesn't exist, then we assume the user/system
         * did a manual 'kill -STOP' / 'kill -CONT' and doesn't intend
         * for fcron to account for any suspend time.
         * This is not considered as an error. */
        if (errno != ENOENT) {
            error_e("Could not open suspend file '%s'", suspendfile);
        }
        goto cleanup_return;
    }

    /* check the file is a 'normal' file (e.g. not a link) and only writable
     * by root -- don't allow attacker to affect job schedules,
     * or delete the suspendfile */
    if (fstat(fd, &s) < 0) {
        error_e("could not fstat() suspend file '%s'", suspendfile);
        goto cleanup_return;
    }
    if (!S_ISREG(s.st_mode) || s.st_nlink != 1) {
        error_e("suspend file %s is not a regular file", suspendfile);
        goto cleanup_return;
    }

    if (s.st_mode & S_IWOTH || s.st_uid != rootuid || s.st_gid != rootgid) {
        error("suspend file %s must be owned by %s:%s and not writable by"
              " others.", suspendfile, ROOTNAME, ROOTGROUP);
        goto cleanup_return;
    }

    /* read the content of the suspendfile into the buffer */
    read_len = read(fd, buf, sizeof(buf) - 1);
    if (read_len < 0) {
        /* we have to run this immediately or errno may be changed */
        error_e("Could not read suspend file '%s'", suspendfile);
        goto unlink_cleanup_return;
    }
    if (read_len < 0) {
        goto unlink_cleanup_return;
    }
    buf[read_len] = '\0';

    errno = 0;
    suspend_duration = strtol(buf, NULL, 10);
    if (errno != 0) {
        error_e("Count not parse suspend duration '%s'", buf);
        suspend_duration = 0;
        goto unlink_cleanup_return;
    }
    else if (suspend_duration < 0) {
        warn("Read negative suspend_duration (%ld): ignoring.");
        suspend_duration = 0;
        goto unlink_cleanup_return;
    }
    else {
        debug("Read suspend_duration of '%ld' from suspend file '%s'",
              suspend_duration, suspendfile);

        if (now < slept_from + suspend_duration) {
            long int time_slept = now - slept_from;

            /* we can have a couple of seconds more due to rounding up,
             * but anything more should be an invalid value in suspendfile */
            explain("Suspend duration %lds in suspend file '%s' is longer than "
                    "we slept.  This could be due to rounding. "
                    "Reverting to time slept %lds.",
                    suspend_duration, suspendfile, time_slept);
            suspend_duration = time_slept;
        }
    }

 unlink_cleanup_return:
    if (unlink(suspendfile) < 0) {
        warn_e("Could not remove suspend file '%s'", suspendfile);
        return 0;
    }

 cleanup_return:
    if (fd >= 0 && xclose(&fd) < 0) {
        warn_e("Could not xclose() suspend file '%s'", suspendfile);
    }

    return suspend_duration;

}

void
init_suspend(select_instance *si)
{

#if defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC)
    linux_init_timeref(&linux_suspend_ref);
#endif                          /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */

#ifdef TFD_TIMER_CANCEL_ON_SET
    {
        struct itimerspec expiration = {
            .it_value.tv_sec = TIME_T_MAX,
        };


        timer_cancel_on_set_fd = timerfd_create(CLOCK_REALTIME, 0);
        if (timer_cancel_on_set_fd == -1) {
            die_e("timerfd_create");
        }
        if (timerfd_settime
            (timer_cancel_on_set_fd,
             TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &expiration,
             NULL) == -1) {
            die_e("timerfd_settime");
        }

    }
    select_add_read(si, timer_cancel_on_set_fd);
#endif                          /* TFD_TIMER_CANCEL_ON_SET */

}


void
check_suspend(time_t slept_from, time_t nwt, bool *sig_cont,
              select_instance *si)
    /* Check if the machine was suspended (to mem or disk), and if so
     * reschedule jobs accordingly */
{
    long int suspend_duration = 0;      /* amount of time the system was suspended */
    long int time_diff;         /* estimate of suspend_duration (as fallback) */

    time_diff = now - nwt;

#ifdef TFD_TIMER_CANCEL_ON_SET
    if (si->retcode > 0 && FD_ISSET(timer_cancel_on_set_fd, &si->readfds)) {
        /* we don't need the data from that fd, but we must read it
         * or select() would return immediately again */
        uint64_t exp;
        read(timer_cancel_on_set_fd, &exp, sizeof(uint64_t));

        debug
            ("We were notified of a change of time, find out suspend duration");
#ifdef JUST_FOR_FORMATTING
    }
#endif

#else                           /* TFD_TIMER_CANCEL_ON_SET */

    if (*sig_cont == true) {
        debug("We received a SIGCONT, find out the suspend duration");

#endif                          /* TFD_TIMER_CANCEL_ON_SET */

#if defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC)
        suspend_duration = linux_get_suspend_time(&linux_suspend_ref);
#else                           /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */
        suspend_duration = read_suspend_duration(slept_from);
#endif                          /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */
    }



    /* Also check if there was an unaccounted sleep duration, in case
     * the OS is not configured to let fcron properly know about suspends
     * via suspendfile.
     * This is not perfect as we may miss some suspend time if fcron
     * is woken up before the timer expiry, e.g. due to a signal
     * or activity on a socket (fcrondyn).
     * NOTE: the +5 second is arbitrary -- just a way to make sure
     * we don't get any false positive.  If the suspend or hibernate
     * is very short it seems fine to simply ignore it anyway */
    if (suspend_duration <= 0 && time_diff > 5) {
#if defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC)
        suspend_duration = linux_get_suspend_time(&linux_suspend_ref);
#else                           /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */
        suspend_duration = time_diff;
#endif                          /* defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC) */
    }

    if (suspend_duration > 0) {
        long int actual_sleep = now - slept_from;
        long int scheduled_sleep = nwt - slept_from;

        explain("suspend/hibernate detected: The system was suspended for "
                "%lus (and we woke up after %lus instead of our intended %lus sleep).",
                suspend_duration, actual_sleep, scheduled_sleep);
        reschedule_all_on_resume(suspend_duration);
    }
}
