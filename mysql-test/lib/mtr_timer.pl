# -*- cperl -*-
# Copyright (C) 2005-2006 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use Socket;
use Errno;
use strict;

sub mtr_init_timers ();
sub mtr_timer_start($$$);
sub mtr_timer_stop($$);
sub mtr_timer_stop_all($);


##############################################################################
#
#  Initiate the structure shared by all timers
#
##############################################################################

sub mtr_init_timers () {
  my $timers = { timers => {}, pids => {}};
  return $timers;
}


##############################################################################
#
#  Start, stop and poll a timer
#
#  As alarm() isn't portable to Windows, we use separate processes to
#  implement timers.
#
##############################################################################

sub mtr_timer_start($$$) {
  my ($timers,$name,$duration)= @_;

  mtr_verbose("mtr_timer_start: $name, $duration");

  if ( exists $timers->{'timers'}->{$name} )
  {
    # We have an old running timer, kill it
    mtr_verbose("There is an old timer running");
    mtr_timer_stop($timers,$name);
  }

 FORK:
  {
    my $tpid= fork();

    if ( ! defined $tpid )
    {
      if ( $! == $!{EAGAIN} )           # See "perldoc Errno"
      {
        mtr_warning("Got EAGAIN from fork(), sleep 1 second and redo");
        sleep(1);
        redo FORK;
      }
      else
      {
        mtr_error("can't fork");
      }
    }

    if ( $tpid )
    {
      # Parent, record the information
      mtr_verbose("timer parent, record info($name, $tpid, $duration)");
      $timers->{'timers'}->{$name}->{'pid'}= $tpid;
      $timers->{'timers'}->{$name}->{'duration'}= $duration;
      $timers->{'pids'}->{$tpid}= $name;
    }
    else
    {
      # Child, redirect output and exec
      # FIXME do we need to redirect streams?

      # Don't do the ^C cleanup in the timeout child processes!
      # There is actually a race here, if we get ^C after fork(), but before
      # clearing the signal handler.
      $SIG{INT}= 'DEFAULT';

      $SIG{TERM}= sub {
	mtr_verbose("timer woke up, exiting!");
	exit(0);
      };

      $0= "mtr_timer(timers,$name,$duration)";
      sleep($duration);
      mtr_verbose("timer expired after $duration seconds");
      exit(0);
    }
  }
}


sub mtr_timer_stop ($$) {
  my ($timers,$name)= @_;

  mtr_verbose("mtr_timer_stop: $name");

  if ( exists $timers->{'timers'}->{$name} )
  {
    my $tpid= $timers->{'timers'}->{$name}->{'pid'};
    mtr_verbose("Stopping timer with pid $tpid");

    # FIXME as Cygwin reuses pids fast, maybe check that is
    # the expected process somehow?!
    kill(15, $tpid);

    # As the timers are so simple programs, we trust them to terminate,
    # and use blocking wait for it. We wait just to avoid a zombie.
    waitpid($tpid,0);

    delete $timers->{'timers'}->{$name}; # Remove the timer information
    delete $timers->{'pids'}->{$tpid};   # and PID reference

    return 1;
  }
  else
  {
    mtr_error("Asked to stop timer \"$name\" not started");
    return 0;
  }
}


sub mtr_timer_stop_all ($) {
  my $timers= shift;

  foreach my $name ( keys %{$timers->{'timers'}} )
  {
    mtr_timer_stop($timers, $name);
  }
  return 1;
}


sub mtr_timer_timeout ($$) {
  my ($timers,$pid)= @_;

  return "" unless exists $timers->{'pids'}->{$pid};

  # We got a timeout, return the name ot the timer
  return $timers->{'pids'}->{$pid};
}

1;
