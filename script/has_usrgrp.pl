#!/usr/bin/perl

# call getpwnam() or getgrnam() to check if user or group (given as arg) exists
#   on the system (getpwnam() and getgrnam() use nsswitch.conf, so it works
#   even if NYS (or similar) is used)

if ( $#ARGV < 1 || $#ARGV > 2) {
    print "usage:\n  has_usrgrp.pl [-user user|-group group] [-printgid]\n";
    exit 1;
}

if ( $ARGV[0] eq "-user" ) {
    ($name, $passwd, $uid, $gid, $quota, $comment,
     $gcos, $dir, $shell, $expire) = getpwnam($ARGV[1]);
}
elsif ( $ARGV[0] eq "-group" ) {
    ($name, $passwd, $gid, $members) = getgrnam($ARGV[1]);
}
else {
    print "usage:\n  has_usrgrp.pl [-user user|-group group] [-printgid]\n";
    exit 1;
}

if ($name) {
    if ( $#ARGV == 2 && $ARGV[2] eq "-printgid" ) {
	print $gid, "\n";
    }
    exit 0;
}
else {
    exit 1;
}
