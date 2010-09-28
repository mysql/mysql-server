# -*- cperl -*-
# Copyright (C) 2004-2006 MySQL AB
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

package My::SafeProcess;

#
# Class that encapsulates process creation, monitoring and cleanup
#
# Spawns a monitor process which spawns a new process locally or
# remote using subclasses My::Process::Local or My::Process::Remote etc.
#
# The monitor process runs a simple event loop more or less just
# waiting for a reason to zap the process it monitors. Thus the user
# of this class does not need to care about process cleanup, it's
# handled automatically.
#
# The monitor process wait for:
#  - the parent process to close the pipe, in that case it
#    will zap the "monitored process" and exit
#  - the "monitored process" to exit, in which case it will exit
#    itself with same exit code as the "monitored process"
#  - the parent process to send the "shutdown" signal in wich case
#    monitor will kill the "monitored process" hard and exit
#
#
# When used it will look something like this:
# $> ps
#  [script.pl]
#   - [monitor for `mysqld`]
#     - [mysqld]
#   - [monitor for `mysqld`]
#     - [mysqld]
#   - [monitor for `mysqld`]
#     - [mysqld]
#
#

use strict;
use Carp;
use POSIX qw(WNOHANG);

use My::SafeProcess::Base;
use base 'My::SafeProcess::Base';

use My::Find;
use My::Platform;

my %running;
my $_verbose= 0;
my $start_exit= 0;

END {
  # Kill any children still running
  for my $proc (values %running){
    if ( $proc->is_child($$) and ! $start_exit){
      #print "Killing: $proc\n";
      if ($proc->wait_one(0)){
	$proc->kill();
      }
    }
  }
}


sub is_child {
  my ($self, $parent_pid)= @_;
  croak "usage: \$safe_proc->is_child()" unless (@_ == 2 and ref $self);
  return ($self->{PARENT} == $parent_pid);
}


my @safe_process_cmd;
my $safe_kill;

# Find the safe process binary or script
sub find_bin {
  if (IS_WIN32PERL or IS_CYGWIN)
  {
    # Use my_safe_process.exe
    my $exe= my_find_bin(".", ["lib/My/SafeProcess", "My/SafeProcess"],
			 "my_safe_process");
    push(@safe_process_cmd, $exe);

    # Use my_safe_kill.exe
    $safe_kill= my_find_bin(".", "lib/My/SafeProcess", "my_safe_kill");
  }
  else
  {
    # Use my_safe_process
    my $exe= my_find_bin(".", ["lib/My/SafeProcess", "My/SafeProcess"],
			 "my_safe_process");
    push(@safe_process_cmd, $exe);
  }
}


sub new {
  my $class= shift;

  my %opts=
    (
     verbose     => 0,
     @_
    );

  my $path     = delete($opts{'path'})    or croak "path required @_";
  my $args     = delete($opts{'args'})    or croak "args required @_";
  my $input    = delete($opts{'input'});
  my $output   = delete($opts{'output'});
  my $error    = delete($opts{'error'});
  my $verbose  = delete($opts{'verbose'});
  my $nocore   = delete($opts{'nocore'});
  my $host     = delete($opts{'host'});
  my $shutdown = delete($opts{'shutdown'});
  my $user_data= delete($opts{'user_data'});

#  if (defined $host) {
#    $safe_script=  "lib/My/SafeProcess/safe_process_cpcd.pl";
#  }

  if (IS_CYGWIN){
    $path= mixed_path($path);
    $input= mixed_path($input);
    $output= mixed_path($output);
    $error= mixed_path($error);
  }

  my @safe_args;
  my ($safe_path, $safe_script)= @safe_process_cmd;
  push(@safe_args, $safe_script) if defined $safe_script;

  push(@safe_args, "--verbose") if $verbose > 0;
  push(@safe_args, "--nocore") if $nocore;

  # Point the safe_process at the right parent if running on cygwin
  push(@safe_args, "--parent-pid=".Cygwin::pid_to_winpid($$)) if IS_CYGWIN;

  push(@safe_args, "--");
  push(@safe_args, $path); # The program safe_process should execute

  if ($start_exit) {	 # Bypass safe_process instead, start program directly
    @safe_args= ();
    $safe_path= $path;
  }
  push(@safe_args, @$$args);

  print "### safe_path: ", $safe_path, " ", join(" ", @safe_args), "\n"
    if $verbose > 1;

  my $pid= create_process(
			  path      => $safe_path,
			  input     => $input,
			  output    => $output,
			  error     => $error,
                          append    => $opts{append},
			  args      => \@safe_args,
			 );

  my $name     = delete($opts{'name'}) || "SafeProcess$pid";
  my $proc= bless
    ({
      SAFE_PID  => $pid,
      SAFE_WINPID  => $pid, # Inidicates this is always a real process
      SAFE_NAME => $name,
      SAFE_SHUTDOWN => $shutdown,
      PARENT => $$,
      SAFE_USER_DATA => $user_data,
     }, $class);

  # Put the new process in list of running
  $running{$pid}= $proc;
  return $proc;

}


sub run {
  my $proc= new(@_);
  $proc->wait_one();
  return $proc->exit_status();
}

#
# Shutdown process nicely, and wait for shutdown_timeout seconds
# If processes hasn't shutdown, kill them hard and wait for return
#
sub shutdown {
  my $shutdown_timeout= shift;
  my @processes= @_;
  _verbose("shutdown, timeout: $shutdown_timeout, @processes");

  return if (@processes == 0);

  # Call shutdown function if process has one, else
  # use kill
  foreach my $proc (@processes){
    _verbose("  proc: $proc");
    my $shutdown= $proc->{SAFE_SHUTDOWN};
    if ($shutdown_timeout > 0 and defined $shutdown){
      $shutdown->();
      $proc->{WAS_SHUTDOWN}= 1;
    }
    else {
      $proc->start_kill();
    }
  }

  my @kill_processes= ();

  # Wait max shutdown_timeout seconds for those process
  # that has been shutdown
  foreach my $proc (@processes){
    next unless $proc->{WAS_SHUTDOWN};
    my $ret= $proc->wait_one($shutdown_timeout);
    if ($ret != 0) {
      push(@kill_processes, $proc);
    }
    # Only wait for the first process with shutdown timeout
    $shutdown_timeout= 0;
  }

  # Wait infinitely for those process
  # that has been killed
  foreach my $proc (@processes){
    next if $proc->{WAS_SHUTDOWN};
    my $ret= $proc->wait_one(undef);
    if ($ret != 0) {
      warn "Wait for killed process failed!";
      push(@kill_processes, $proc);
      # Try one more time, best option...
    }
  }

  # Return if all servers has exited
  return if (@kill_processes == 0);

  foreach my $proc (@kill_processes){
    $proc->start_kill();
  }

  foreach my $proc (@kill_processes){
    $proc->wait_one(undef);
  }

  return;
}


sub _winpid ($) {
  my ($pid)= @_;

  # In win32 perl, the pid is already the winpid
  return $pid unless IS_CYGWIN;

  # In cygwin, the pid is the pseudo process ->
  # get the real winpid of my_safe_process
  return Cygwin::pid_to_winpid($pid);
}


#
# Tell the process to die as fast as possible
#
sub start_kill {
  my ($self)= @_;
  croak "usage: \$safe_proc->start_kill()" unless (@_ == 1 and ref $self);
  _verbose("start_kill: $self");
  my $ret= 1;

  my $pid= $self->{SAFE_PID};
  die "INTERNAL ERROR: no pid" unless defined $pid;

  if (IS_WINDOWS and defined $self->{SAFE_WINPID})
  {
    die "INTERNAL ERROR: no safe_kill" unless defined $safe_kill;

    my $winpid= _winpid($pid);
    $ret= system($safe_kill, $winpid) >> 8;

    if ($ret == 3){
      print "Couldn't open the winpid: $winpid ".
	"for pid: $pid, try one more time\n";
      sleep(1);
      $winpid= _winpid($pid);
      $ret= system($safe_kill, $winpid) >> 8;
      print "Couldn't open the winpid: $winpid ".
	"for pid: $pid, continue and see what happens...\n";
    }
  }
  else
  {
    $pid= $self->{SAFE_PID};
    die "Can't kill not started process" unless defined $pid;
    $ret= kill("TERM", $pid);
  }

  return $ret;
}


sub dump_core {
  my ($self)= @_;
  return if IS_WINDOWS;
  my $pid= $self->{SAFE_PID};
  die "Can't cet core from not started process" unless defined $pid;
  _verbose("Sending ABRT to $self");
  kill ("ABRT", $pid);
  return 1;
}


#
# Kill the process as fast as possible
# and wait for it to return
#
sub kill {
  my ($self)= @_;
  croak "usage: \$safe_proc->kill()" unless (@_ == 1 and ref $self);

  $self->start_kill();
  $self->wait_one();
  return 1;
}


sub _collect {
  my ($self)= @_;

  $self->{EXIT_STATUS}= $?;
  _verbose("_collect: $self");

  # Take the process out of running list
  my $pid= $self->{SAFE_PID};
  die unless delete($running{$pid});
}


# Wait for process to exit
# optionally with a timeout
#
# timeout
#   undef -> wait blocking infinitely
#   0     -> just poll with WNOHANG
#   >0    -> wait blocking for max timeout seconds
#
# RETURN VALUES
#  0 Not running
#  1 Still running
#
sub wait_one {
  my ($self, $timeout)= @_;
  croak "usage: \$safe_proc->wait_one([timeout])" unless ref $self;

  _verbose("wait_one $self, $timeout");

  if ( ! defined($self->{SAFE_PID}) ) {
    # No pid => not running
    _verbose("No pid => not running");
    return 0;
  }

  if ( defined $self->{EXIT_STATUS} ) {
    # Exit status already set => not running
    _verbose("Exit status already set => not running");
    return 0;
  }

  my $pid= $self->{SAFE_PID};

  my $use_alarm;
  my $blocking;
  if (defined $timeout)
  {
    if ($timeout == 0)
    {
      # 0 -> just poll with WNOHANG
      $blocking= 0;
      $use_alarm= 0;
    }
    else
    {
      # >0 -> wait blocking for max timeout seconds
      $blocking= 1;
      $use_alarm= 1;
    }
  }
  else
  {
    # undef -> wait blocking infinitely
    $blocking= 1;
    $use_alarm= 0;
  }
  #_verbose("blocking: $blocking, use_alarm: $use_alarm");

  my $retpid;
  eval
  {
    # alarm should break the wait
    local $SIG{ALRM}= sub { die "waitpid timeout"; };

    alarm($timeout) if $use_alarm;

    $retpid= waitpid($pid, $blocking ? 0 : &WNOHANG);

    alarm(0) if $use_alarm;
  };

  if ($@)
  {
    die "Got unexpected: $@" if ($@ !~ /waitpid timeout/);
    if (!defined $retpid) {
      # Got timeout
      _verbose("Got timeout");
      return 1;
    }
    # Got pid _and_ alarm, continue
    _verbose("Got pid and alarm, continue");
  }

  if ( $retpid == 0 ) {
    # 0 => still running
    _verbose("0 => still running");
    return 1;
  }

  if ( not $blocking and $retpid == -1 ) {
    # still running
    _verbose("still running");
    return 1;
  }

  #warn "wait_one: expected pid $pid but got $retpid"
  #  unless( $retpid == $pid );

  $self->_collect();
  return 0;
}


#
# Wait for any process to exit
#
# Returns a reference to the SafeProcess that
# exited or undefined
#
sub wait_any {
  my $ret_pid;
  if (IS_WIN32PERL) {
    # Can't wait for -1 => use a polling loop
    do {
      Win32::Sleep(10); # 10 milli seconds
      foreach my $pid (keys %running){
	$ret_pid= waitpid($pid, &WNOHANG);
	last if $pid == $ret_pid;
      }
    } while ($ret_pid == 0);
  }
  else
  {
    $ret_pid= waitpid(-1, 0);
    if ($ret_pid <= 0){
      # No more processes to wait for
      print STDERR "wait_any, got invalid pid: $ret_pid\n";
      return undef;
    }
  }

  # Look it up in "running" table
  my $proc= $running{$ret_pid};
  unless (defined $proc){
    print STDERR "Could not find pid: $ret_pid in running list\n";
    print STDERR "running: ". join(", ", keys(%running)). "\n";
    return undef;
  }
  $proc->_collect;
  return $proc;
}


#
# Wait for any process to exit, or a timeout
#
# Returns a reference to the SafeProcess that
# exited or a pseudo-process with $proc->{timeout} == 1
#

sub wait_any_timeout {
  my $class= shift;
  my $timeout= shift;
  my $proc;
  my $millis=10;

  do {
    ::mtr_milli_sleep($millis);
    # Slowly increse interval up to max. 1 second
    $millis++ if $millis < 1000;
    # Return a "fake" process for timeout
    if (::has_expired($timeout)) {
      $proc= bless
	({
	  SAFE_PID  => 0,
	  SAFE_NAME => "timer",
	  timeout => 1,
	 }, $class);
    } else {
      $proc= check_any();
    }
  } while (! $proc);

  return $proc;
}


#
# Wait for all processes to exit
#
sub wait_all {
  while(keys %running)
  {
    wait_any();
  }
}

#
# Set global flag to tell all safe_process to exit after starting child
#

sub start_exit {
  $start_exit= 1;
}

#
# Check if any process has exited, but don't wait.
#
# Returns a reference to the SafeProcess that
# exited or undefined
#
sub check_any {
  for my $proc (values %running){
    if ( $proc->is_child($$) ) {
      if (not $proc->wait_one(0)) {
	_verbose ("Found exited $proc");
	return $proc;
      }
    }
  }
  return undef;
}


# Overload string operator
# and fallback to default functions if no
# overloaded function is found
#
use overload
  '""' => \&self2str,
  fallback => 1;


#
# Return the process as a nicely formatted string
#
sub self2str {
  my ($self)= @_;
  my $pid=  $self->{SAFE_PID};
  my $winpid=  $self->{SAFE_WINPID};
  my $name= $self->{SAFE_NAME};
  my $exit_status= $self->{EXIT_STATUS};

  my $str= "[$name - pid: $pid";
  $str.= ", winpid: $winpid"      if defined $winpid;
  $str.= ", exit: $exit_status"   if defined $exit_status;
  $str.= "]";
}

sub _verbose {
  return unless $_verbose;
  print STDERR " ## ". @_. "\n";
}


sub pid {
  my ($self)= @_;
  return $self->{SAFE_PID};
}

sub user_data {
  my ($self)= @_;
  return $self->{SAFE_USER_DATA};
}


1;
