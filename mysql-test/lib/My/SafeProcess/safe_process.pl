#!/usr/bin/perl
# -*- cperl -*-

# Copyright (c) 2007, 2011, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

use strict;
use warnings;

use lib 'lib';
use My::SafeProcess::Base;
use POSIX qw(WNOHANG);

###########################################################################
# Util functions
###########################################################################

#
#Print message to stderr
#
my $verbose= 0;
sub message {
  if ($verbose > 0){
    use Time::localtime;
    my $tm= localtime();
    my $timestamp= sprintf("%02d%02d%02d %2d:%02d:%02d",
			   $tm->year % 100, $tm->mon+1, $tm->mday,
			   $tm->hour, $tm->min, $tm->sec);
    print STDERR $timestamp, " monitor[$$]: ", @_, "\n";
  }
}


###########################################################################
# Main program
###########################################################################

my $terminated= 0;

# Protect against being killed in the middle
# of child creation, just set the terminated flag
# to make sure the child will be killed off
# when program is ready to do that
$SIG{TERM}= sub { message("!Got signal @_"); $terminated= 1; };
$SIG{INT}= sub { message("!Got signal @_"); $terminated= 1; };

my $parent_pid= getppid();

my $found_double_dash= 0;
while (my $arg= shift(@ARGV)){

  if ($arg =~ /^--$/){
    $found_double_dash= 1;
    last;
  }
  elsif ($arg =~ /^--verbose$/){
    $verbose= 1;
  }
  else {
    die "Unknown option: $arg";
  }
}

my $path=       shift(@ARGV); # Executable

die "usage:\n" .
    " safe_process.pl [opts] -- <path> [<args> [...<args_n>]]"
  unless defined $path || $found_double_dash;


message("started");
#message("path: '$path'");
message("parent: $parent_pid");

# Start process to monitor
my $child_pid=
  create_process(
		 path     => $path,
		 args     => \@ARGV,
		 setpgrp  => 1,
		);
message("Started child $child_pid");

eval {
  sub handle_signal {
    $terminated= 1;
    message("Got signal @_");

    # Ignore all signals
    foreach my $name (keys %SIG){
      $SIG{$name}= 'IGNORE';
    }

    die "signaled\n";
  };
  local $SIG{TERM}= \&handle_signal;
  local $SIG{INT}=  \&handle_signal;
  local $SIG{CHLD}= sub {
    message("Got signal @_");
    kill('KILL', -$child_pid);
    my $ret= waitpid($child_pid, 0);
    if ($? & 127){
      exit(65); # Killed by signal
    }
    exit($? >> 8);
  };

  # Monitoring loop
  while(!$terminated) {

    # Check if parent is still alive
    if (kill(0, $parent_pid) < 1){
      message("Parent is not alive anymore");
      last;
    }

    # Wait for child to terminate but wakeup every
    # second to also check that parent is still alive
    my $ret_pid;
    $ret_pid= waitpid($child_pid, &WNOHANG);
    if ($ret_pid == $child_pid) {
      # Process has exited, collect return status
      my $ret_code= $? >> 8;
      message("Child exit: $ret_code");
      # Exit with exit status of the child
      exit ($ret_code);
    }
    sleep(1);
  }
};
if ( $@ ) {
  # The monitoring loop should have been
  # broken by handle_signal
  warn "Unexpected: $@" unless ( $@ =~ /signaled/ );
}

# Use negative pid in order to kill the whole
# process group
#
my $ret= kill('KILL', -$child_pid);
message("Killed child: $child_pid, ret: $ret");
if ($ret > 0) {
  message("Killed child: $child_pid");
  # Wait blocking for the child to return
  my $ret_pid= waitpid($child_pid, 0);
  if ($ret_pid != $child_pid){
    message("unexpected pid $ret_pid returned from waitpid($child_pid)");
  }
}

message("DONE!");
exit (1);


