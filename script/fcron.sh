#!/bin/sh
# Start fcron at boot time under FreeBSD

SBIN=@@DESTSBIN@

case "$1" in
  start)
    $SBIN/fcron -b && echo -n " fcron"
    ;;
  stop)
    killall -TERM fcron
    ;;
  *)
    echo "Usage: fcron start|stop"
    exit 1
    ;;
esac