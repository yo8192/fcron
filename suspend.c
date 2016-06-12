/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2014 Thibault Godouet <fcron@free.fr>
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
check_suspend(time_t slept_from, time_t nwt, char *sig_cont)
    /* Check if the machine was suspended (to mem or disk), and if so
     * reschedule jobs accordingly */
{
    long int suspend_duration;  /* amount of time the system was suspended */
    long int time_diff;         /* estimate of suspend_duration (as fallback) */

    if (*sig_cont > 0) {
        /* the signal CONT was raised, check the suspendfile */
        suspend_duration = read_suspend_duration(slept_from);
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
    time_diff = now - nwt;
    if (suspend_duration <= 0 && time_diff > 5) {
        suspend_duration = time_diff;
    }

    if (suspend_duration > 0) {
        long int actual_sleep = now - slept_from;
        long int scheduled_sleep = nwt - slept_from;
        explain("suspend/hibernate detected: we woke up after %lus"
                " instead of %lus. The system was suspended for %lus.",
                actual_sleep, scheduled_sleep, suspend_duration);
        reschedule_all_on_resume(suspend_duration);
    }
}
