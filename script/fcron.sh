#!/bin/sh
# Start fcron at boot time under FreeBSD

SBIN=@@DESTSBIN@

case "$1" in
  start)
    $SBIN/fcron -b && echo -n "fcron"
    ;;
  *)
    echo "Usage: fcron start"
    exit 1
    ;;
esac