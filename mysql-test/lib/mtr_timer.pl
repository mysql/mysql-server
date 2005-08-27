# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use Carp qw(cluck);
use Socket;
use Errno;
use strict;

#use POSIX ":sys_wait_h";
use POSIX 'WNOHANG';

sub mtr_init_timers ();
sub mtr_timer_start($$$);
sub mtr_timer_stop($$);
sub mtr_timer_stop_all($);
sub mtr_timer_waitpid($$$);

##############################################################################
#
#  Initiate a structure shared by all timers
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
#  implement timers. That is why there is a mtr_timer_waitpid(), as this
#  is where we catch a timeout.
#
##############################################################################

sub mtr_timer_start($$$) {
  my ($timers,$name,$duration)= @_;

  if ( exists $timers->{'timers'}->{$name} )
  {
    # We have an old running timer, kill it
    mtr_timer_stop($timers,$name);
  }

 FORK:
  {
    my $tpid= fork();

    if ( ! defined $tpid )
    {
      if ( $! == $!{EAGAIN} )           # See "perldoc Errno"
      {
        mtr_debug("Got EAGAIN from fork(), sleep 1 second and redo");
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
      $timers->{'timers'}->{$name}->{'pid'}= $tpid;
      $timers->{'timers'}->{$name}->{'duration'}= $duration;
      $timers->{'pids'}->{$tpid}= $name;
    }
    else
    {
      # Child, redirect output and exec
      # FIXME do we need to redirect streams?
      $0= "mtr_timer(timers,$name,$duration)";
      sleep($duration);
      exit(0);
    }
  }
}


sub mtr_timer_stop ($$) {
  my ($timers,$name)= @_;

  if ( exists $timers->{'timers'}->{$name} )
  {
    my $tpid= $timers->{'timers'}->{$name}->{'pid'};

    # FIXME as Cygwin reuses pids fast, maybe check that is
    # the expected process somehow?!
    kill(9, $tpid);

    # As the timers are so simple programs, we trust them to terminate,
    # and use blocking wait for it. We wait just to avoid a zombie.
    waitpid($tpid,0);

    delete $timers->{'timers'}->{$name}; # Remove the timer information
    delete $timers->{'pids'}->{$tpid};   # and PID reference

    return 1;
  }
  else
  {
    mtr_debug("Asked to stop timer \"$name\" not started");
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

  # We got a timeout
  my $name= $timers->{'pids'}->{$pid};
  mtr_timer_stop($timers, $timers->{'timers'}->{$name});
  return $name;
}

1;
