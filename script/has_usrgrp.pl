#!/usr/bin/perl

# call getpwnam() or getgrnam() to check if user or group (given as arg) exists
#   on the system (getpwnam() and getgrnam() use nsswitch.conf, so it works
#   even if NYS (or similar) is used)

sub usage {
    return "usage:\n  has_usrgrp.pl [-user user|-group group] [-printgid]\n";
}

if ( @ARGV < 2 || @ARGV > 3) {
    die usage();
}

if ( $ARGV[0] eq "-user" ) {
    ($name, $passwd, $uid, $gid) = getpwnam($ARGV[1]);
}
elsif ( $ARGV[0] eq "-group" ) {
    ($name, $passwd, $gid) = getgrnam($ARGV[1]);
}
else {
    die usage();
}

if ($name) {
    if ( @ARGV == 3 && $ARGV[2] eq "-printgid" ) {
	print $gid, "\n";
    }
    exit 0;
}
else {
    exit 1;
}
