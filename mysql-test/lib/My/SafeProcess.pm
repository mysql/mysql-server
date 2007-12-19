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

use POSIX qw(WNOHANG);

use My::SafeProcess::Base;
use base 'My::SafeProcess::Base';

use My::Find;

my %running;

BEGIN {
  if ($^O eq "MSWin32") {
    eval 'sub IS_WIN32PERL () { 1 }';
  }
  else {
    eval 'sub IS_WIN32PERL () { 0 }';
  }
  if ($^O eq "cygwin") {
    eval 'sub IS_CYGWIN () { 1 }';
    # Make sure cygpath works
    if ((system("cygpath > /dev/null 2>&1") >> 8) != 1){
      die "Could not execute 'cygpath': $!";
    }
    eval 'sub fixpath {
            my ($path)= @_;
            return unless defined $path;
            $path= `cygpath -w $path`;
            chomp $path;
            return $path;
          }';
  }
  else {
    eval 'sub IS_CYGWIN () { 0 }';
  }
}

# Find the safe process binary or script
my @safe_process_cmd;
my $safe_kill;
if (IS_WIN32PERL or IS_CYGWIN){
  # Use my_safe_process.exe
  my $exe= my_find_bin(".", "lib/My/SafeProcess", "my_safe_process.exe");
  die "Could not find my_safe_process.exe" unless $exe;
  push(@safe_process_cmd, $exe);

  # Use my_safe_kill.exe
  my $safe_kill= my_find_bin(".", "lib/My/SafeProcess", "my_safe_kill");
  die "Could not find my_safe_kill.exe" unless $safe_kill;
}
else {
  # Use safe_process.pl
  my $script=  "lib/My/SafeProcess/safe_process.pl";
  $script= "../$script" unless -f $script;
  die "Could not find safe_process.pl" unless -f $script;

  # Call $script with Perl interpreter
  push(@safe_process_cmd, $^X, $script);
}


sub new {
  my $class= shift;

  my %opts=
    (
     verbose     => 0,
     @_
    );

  my $path     = delete($opts{'path'})    or die "path required";
  my $args     = delete($opts{'args'})    or die "args required";
  my $input    = delete($opts{'input'});
  my $output   = delete($opts{'output'});
  my $error    = delete($opts{'error'});
  my $verbose  = delete($opts{'verbose'});
  my $host     = delete($opts{'host'});
  my $shutdown = delete($opts{'shutdown'});

#  if (defined $host) {
#    $safe_script=  "lib/My/SafeProcess/safe_process_cpcd.pl";
#  }

  if (IS_CYGWIN){
    # safe_procss is a windows program and need
    # windows paths
    $path= fixpath($path);
    $input= fixpath($input);
    $output= fixpath($output);
    $error= fixpath($error);
  }

  my @safe_args;
  my ($safe_path, $safe_script)= @safe_process_cmd;
  push(@safe_args, $safe_script) if defined $safe_script;

  push(@safe_args, "--verbose") if $verbose > 0;

  # Point the safe_process at the right parent if running on cygwin
  push(@safe_args, "--parent-pid=".Cygwin::pid_to_winpid($$)) if IS_CYGWIN;

  push(@safe_args, "--");
  push(@safe_args, $path); # The program safe_process should execute
  push(@safe_args, @$$args);

  print "### safe_path: ", $safe_path, " ", join(" ", @safe_args), "\n"
    if $verbose > 1;

  my ($pid, $winpid)= create_process(
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
      SAFE_WINPID  => $winpid,
      SAFE_NAME => $name,
      SAFE_SHUTDOWN => $shutdown,
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
# Start a process that returns after "duration" seconds
# or when it's parent process does not exist anymore
#
sub timer {
  my $class= shift;
  my $duration= shift or die "duration required";
  my $parent_pid= $$;

  my $pid= My::SafeProcess::Base::_safe_fork();
  if ($pid){
    # Parent
    my $proc= bless
      ({
	SAFE_PID  => $pid,
	SAFE_NAME => "timer",
       }, $class);

    # Put the new process in list of running
    $running{$pid}= $proc;
    return $proc;
  }

  # Child, install signal handlers and sleep for "duration"
  $SIG{INT}= 'DEFAULT';

  $SIG{TERM}= sub {
    #print STDERR "timer $$: woken up, exiting!\n";
    exit(0);
  };

  $0= "safe_timer($duration)";
  my $count_down= $duration;
  while($count_down--){

    # Check that parent is still alive
    if (kill(0, $parent_pid) == 0){
      #print STDERR "timer $$: parent gone, exiting!\n";
      exit(0);
    }

    sleep(1);
  }
  print STDERR "timer $$: expired after $duration seconds\n";
  exit(0);
}


#
# Shutdown process nicely, and wait for shutdown_timeout seconds
# If processes hasn't shutdown, kill them hard and wait for return
#
sub shutdown {
  my $shutdown_timeout= shift;
  my @processes= @_;

  return if (@processes == 0);

  #print "shutdown: @processes\n";

  # Call shutdown function if process has one, else
  # use kill
  foreach my $proc (@processes){
    my $shutdown= $proc->{SAFE_SHUTDOWN};
    if ($shutdown_timeout > 0 and defined $shutdown){
      $shutdown->();
    }
    else {
      $proc->start_kill();
    }
  }

  my @kill_processes= ();

  # Wait for shutdown_timeout for processes to exit
  foreach my $proc (@processes){
    my $ret= $proc->wait_one($shutdown_timeout);
    if ($ret != 0) {
      push(@kill_processes, $proc);
    }
    # Only wait for the first process with shutdown timeout
    $shutdown_timeout= 0;
  }

  # Return if all servers has exited
  return if (@kill_processes == 0);

  foreach my $proc (@kill_processes){
    $proc->start_kill();
  }

  foreach my $proc (@kill_processes){
    $proc->wait_one();
  }
  return;
}


#
# Tell the process to die as fast as possible
#
sub start_kill {
  my ($self)= @_;
  die "usage: \$safe_proc->start_kill()" unless (@_ == 1 and ref $self);
  #print "start_kill $self\n";

  if (defined $safe_kill and $self->{SAFE_WINPID}){
    # Use my_safe_kill to tell my_safe_process
    # it's time to kill it's child and return
    my $pid= $self->{SAFE_WINPID};
    my $ret= system($safe_kill, $pid);
    #print STDERR "start_kill, safe_killed $pid, ret: $ret\n";
  } else {
    my $pid= $self->{SAFE_PID};
    die "Can't kill not started process" unless defined $pid;
    my $ret= kill(15, $pid);
    #print STDERR "start_kill, sent signal 15 to $pid, ret: $ret\n";
  }
  return 1;
}


#
# Kill the process as fast as possible
# and wait for it to return
#
sub kill {
  my ($self)= @_;
  die "usage: \$safe_proc->kill()" unless (@_ == 1 and ref $self);

  $self->start_kill();
  $self->wait_one();
  return 1;
}


sub _collect {
  my ($self)= @_;

  #print "_collect\n";
  $self->{EXIT_STATUS}= $?;

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
  die "usage: \$safe_proc->wait_one([timeout])" unless ref $self;

  #print "wait_one $self, $timeout\n";

  if ( ! defined($self->{SAFE_PID}) ) {
    # No pid => not running
    return 0;
  }

  if ( defined $self->{EXIT_STATUS} ) {
    # Exit status already set => not running
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
      return 1;
    }
    # Got pid _and_ alarm, continue
  }

  if ( $retpid == 0 ) {
    # 0 => still running
    return 1;
  }

  if ( not $blocking and $retpid == -1 ) {
    # still running
    return 1;
  }

  warn "wait_one: expected pid $pid but got $retpid"
    unless( $retpid == $pid );

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

    # Special processig of return code
    # since negative pids are valid
    if ($ret_pid == 0 or $ret_pid == -1) {
      print STDERR "wait_any, got invalid pid: $ret_pid\n";
      return undef;
    }
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
    print STDERR "Could not find pid in running list\n";
    print STDERR "running: ". join(", ", keys(%running)). "\n";
    return undef;
  }
  $proc->_collect;
  return $proc;
}

#
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


1;
