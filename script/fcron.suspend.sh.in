#!/bin/sh
#
# Suspend script for fcron.
# Designed to work under systemd and pm-utils.
#
# Install as:
# - systemd: /usr/lib/systemd/system-sleep/fcron.sh
# - pm-utils: /etc/pm/sleep.d/74_fcron
#

PID_FILE=/usr/local/var/run/fcron.pid
SUSPEND_FILE=/usr/local/var/run/fcron.suspend
PATH=/bin:/usr/bin:/sbin:/usr/sbin
LOGGER="logger -p cron.info"

FCRONPID=`cat $PID_FILE`

# pm-utils: first argument will be hibernate|suspend|resume|thaw
# systemd: two arguments: 1st one is pre|post, second one is suspend|hibernate|hybrid-sleep

case $1 in
     pre|suspend|suspend_hybrid|hibernate)
        # We will use the modify time of SUSPEND_FILE to know when we went into suspend:
        kill -STOP "$FCRONPID" || $LOGGER "$0 $*: could not stop fcron pid '$FCRONPID'"
	touch $SUSPEND_FILE || $LOGGER "$0 $*: could not touch fcron suspend file '$SUSPEND_FILE'"
        $LOGGER "$0 $*: stopped pid `cat $PID_FILE` (from $PID_FILE), and touched $SUSPEND_FILE"
        ;;
     post|resume|thaw)
	SLEEP_FROM=`stat -c %Y $SUSPEND_FILE`
        if test $? -eq 0; then
            NOW=`date +%s`
            SLEEP_DURATION=`expr $NOW - $SLEEP_FROM`
            if test $? -lt 2; then
                $LOGGER "$0 $*: SLEEP_DURATION=$SLEEP_DURATION"
                echo $SLEEP_DURATION > $SUSPEND_FILE
            else
                # something went wrong -- resume fcron without specifying
                # a suspend duration as it may be wrong
                $LOGGER "$0 $*: could not compute sleep duration"
                rm -f $SUSPEND_FILE
            fi
        else
            # something went wrong -- resume fcron without specifying
            # a suspend duration as it may be wrong
            $LOGGER "$0 $*: could not stat $SUSPEND_FILE"
            rm -f $SUSPEND_FILE
        fi
        $LOGGER "$0 $*: resuming pid `cat $PID_FILE` (from $PID_FILE)"
        kill -CONT `cat $PID_FILE` || $LOGGER "$0 $*: could not resume fcron pid '$FCRONPID'"
        ;;
    *)
        $LOGGER "$0 $*: invalid argument."
esac
