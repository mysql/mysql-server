# -*- cperl -*-
# Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

package My::SafeProcess;

# Class that encapsulates process creation, monitoring and cleanup.
#
# Spawns a monitor process which spawns a new process locally or remote
# using subclasses My::Process::Local or My::Process::Remote etc.
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

use My::Find;
use My::Platform;
use My::SafeProcess::Base;
use base 'My::SafeProcess::Base';

my %running;
my $_verbose   = 0;
my $start_exit = 0;

my $bindir;
my $safe_kill;
my @safe_process_cmd;
if (defined $ENV{MTR_BINDIR}) {
  # This is an out-of-source build. Build directory
  # is given in MTR_BINDIR env.variable
  $bindir = $ENV{MTR_BINDIR} . "/mysql-test";
} else {
  $bindir = ".";
}

END {
  # Kill any children still running
  for my $proc (values %running) {
    if ($proc->is_child($$) and !$start_exit) {
      #print "Killing: $proc\n";
      if ($proc->wait_one(0)) {
        $proc->kill();
      }
    }
  }
}

sub is_child {
  my ($self, $parent_pid) = @_;
  croak "usage: \$safe_proc->is_child()" unless (@_ == 2 and ref $self);
  return ($self->{PARENT} == $parent_pid);
}

# Find the safe process binary or script
sub find_bin {
  if (IS_WIN32PERL or IS_CYGWIN) {
    # Use my_safe_process.exe
    my $exe = my_find_bin($bindir, [ "lib/My/SafeProcess", "My/SafeProcess" ],
                          "my_safe_process");
    push(@safe_process_cmd, $exe);

    # Use my_safe_kill.exe
    $safe_kill = my_find_bin($bindir, "lib/My/SafeProcess", "my_safe_kill");
  } else {
    # Use my_safe_process
    my $exe = my_find_bin($bindir, [ "lib/My/SafeProcess", "My/SafeProcess" ],
                          "my_safe_process");
    push(@safe_process_cmd, $exe);
  }
}

sub new {
  my $class = shift;

  my %opts = (verbose => 0,
              @_);

  my $args        = delete($opts{'args'}) or croak "args required @_";
  my $daemon_mode = delete($opts{'daemon_mode'});
  my $envs        = delete($opts{'envs'});
  my $error       = delete($opts{'error'});
  my $host        = delete($opts{'host'});
  my $input       = delete($opts{'input'});
  my $nocore      = delete($opts{'nocore'});
  my $output      = delete($opts{'output'});
  my $path        = delete($opts{'path'}) or croak "path required @_";
  my $pid_file    = delete($opts{'pid_file'})
    if defined $daemon_mode && $daemon_mode == 1;
  my $shutdown  = delete($opts{'shutdown'});
  my $user_data = delete($opts{'user_data'});
  my $verbose   = delete($opts{'verbose'});

  if (IS_CYGWIN) {
    $error  = mixed_path($error);
    $input  = mixed_path($input);
    $output = mixed_path($output);
    $path   = mixed_path($path);
  }

  my @safe_args;
  my ($safe_path, $safe_script) = @safe_process_cmd;
  push(@safe_args, $safe_script) if defined $safe_script;

  push(@safe_args, "--verbose") if $verbose;
  push(@safe_args, "--nocore")  if $nocore;

  # Point the safe_process at the right parent if running on cygwin
  push(@safe_args, "--parent-pid=" . Cygwin::pid_to_winpid($$)) if IS_CYGWIN;

  foreach my $env_var (@$envs) {
    croak("Missing = in env string") unless $env_var =~ /=/;
    croak("Env string $env_var seen, probably missing value for --mysqld-env")
      if $env_var =~ /^--/;
    push @safe_args, "--env $env_var";
  }

  push(@safe_args, "--");
  push(@safe_args, $path);

  if ($start_exit) {
    # Bypass safe_process instead, start program directly
    @safe_args = ();
    $safe_path = $path;
  }
  push(@safe_args, @$$args);

  if ($verbose) {
    print "### safe_path: ", $safe_path, " ", join(" ", @safe_args), "\n";
  }

  my $pid = create_process(append   => $opts{append},
                           args     => \@safe_args,
                           error    => $error,
                           input    => $input,
                           output   => $output,
                           path     => $safe_path,
                           pid_file => $pid_file,);

  my $name = delete($opts{'name'}) || "SafeProcess$pid";
  my $proc = bless({ PARENT         => $$,
                     SAFE_NAME      => $name,
                     SAFE_PID       => $pid,
                     SAFE_SHUTDOWN  => $shutdown,
                     SAFE_USER_DATA => $user_data,
                     SAFE_WINPID    => $pid,
                   },
                   $class);
  # Put the new process in list of running
  $running{$pid} = $proc;
  return $proc;
}

sub run {
  my $proc = new(@_);
  $proc->wait_one();
  return $proc->exit_status();
}

# Shutdown process nicely, and wait for shutdown_timeout seconds.
# If processes hasn't shutdown, kill them hard and wait for return.
sub shutdown {
  my $shutdown_timeout = shift;
  my @processes        = @_;
  _verbose("shutdown, timeout: $shutdown_timeout, @processes");

  return if (@processes == 0);

  # Call shutdown function if process has one, else use kill.
  foreach my $proc (@processes) {
    _verbose("  proc: $proc");
    my $shutdown = $proc->{SAFE_SHUTDOWN};
    if ($shutdown_timeout > 0 and defined $shutdown) {
      $shutdown->();
      $proc->{WAS_SHUTDOWN} = 1;
    } else {
      $proc->start_kill();
    }
  }

  my @kill_processes = ();

  # Wait max shutdown_timeout seconds for those process
  # that has been shutdown.
  foreach my $proc (@processes) {
    next unless $proc->{WAS_SHUTDOWN};
    my $ret = $proc->wait_one($shutdown_timeout);

    if ($ret != 0) {
      push(@kill_processes, $proc);
    }
    # Only wait for the first process with shutdown timeout
    $shutdown_timeout = 0;
  }

  # Wait infinitely for those process that has been killed
  foreach my $proc (@processes) {
    next if $proc->{WAS_SHUTDOWN};
    my $ret = $proc->wait_one(undef);

    if ($ret != 0) {
      warn "Wait for killed process failed!";
      push(@kill_processes, $proc);
      # Try one more time, best option...
    }
  }

  # Return if all servers has exited
  return if (@kill_processes == 0);

  foreach my $proc (@kill_processes) {
    $proc->start_kill();
  }

  foreach my $proc (@kill_processes) {
    $proc->wait_one(undef);
  }

  return;
}

sub _winpid ($) {
  my ($pid) = @_;

  # In win32 perl, the pid is already the winpid
  return $pid unless IS_CYGWIN;

  # In cygwin, the pid is the pseudo process ->
  # get the real winpid of my_safe_process
  return Cygwin::pid_to_winpid($pid);
}

# Tell the process to die as fast as possible
sub start_kill {
  my ($self) = @_;
  croak "usage: \$safe_proc->start_kill()" unless (@_ == 1 and ref $self);

  _verbose("start_kill: $self");

  my $pid = $self->{SAFE_PID};
  die "INTERNAL ERROR: no pid" unless defined $pid;

  my $ret = 1;
  if (IS_WINDOWS and defined $self->{SAFE_WINPID}) {
    die "INTERNAL ERROR: no safe_kill" unless defined $safe_kill;

    my $winpid = _winpid($pid);
    $ret = system($safe_kill, $winpid) >> 8;

    if ($ret == 3) {
      print "Couldn't open the winpid: $winpid  for pid: $pid, " .
        "try one more time\n";
      sleep(1);
      $winpid = _winpid($pid);
      $ret = system($safe_kill, $winpid) >> 8;
      print "Couldn't open the winpid: $winpid for pid: $pid, " .
        "continue and see what happens...\n";
    }
  } else {
    $pid = $self->{SAFE_PID};
    die "Can't kill not started process" unless defined $pid;
    $ret = kill("TERM", $pid);
  }

  return $ret;
}

sub dump_core {
  my ($self) = @_;
  my $pid = $self->{SAFE_PID};
  die "Can't get core from not started process" unless defined $pid;
  _verbose("Sending ABRT to $self");
  kill("ABRT", $pid);
  return 1;
}

sub dump_core_windows {
  my ($self, $mysqld, $call_cdb) = @_;

  # Check if cdb utility should be called or not
  if ($call_cdb) {
    # Check whether cdb debugging tool is installed
    if (My::CoreDump->cdb_check()) {
      # Fetch the PID of mysqld process
      open FILE, $mysqld->value('pid-file');
      chomp(my $pid = <FILE>);
      close FILE;

      # Generating core dump of mysqld process
      my $core_name =
        $mysqld->value('datadir') . "/" . $mysqld->name() . ".dmp";
      `cdb -pv -p $pid -c \".dump /m $core_name;q\" 2>&1`;
    }
  }

  $self->kill();
}

# Kill the process as fast as possible and wait for it to return.
sub kill {
  my ($self) = @_;
  croak "usage: \$safe_proc->kill()" unless (@_ == 1 and ref $self);

  $self->start_kill();
  $self->wait_one();
  return 1;
}

sub _collect {
  my ($self) = @_;

  $self->{EXIT_STATUS} = $?;
  _verbose("_collect: $self");

  # Take the process out of running list
  my $pid = $self->{SAFE_PID};
  die unless delete($running{$pid});
}

# Wait for process to exit optionally with a timeout.
#
# timeout
#   undef -> wait blocking infinitely
#   0     -> just poll with WNOHANG
#   >0    -> wait blocking for max timeout seconds
#
# RETURN VALUES
#   0 Not running
#   1 Still running
sub wait_one {
  my ($self, $timeout) = @_;
  croak "usage: \$safe_proc->wait_one([timeout])" unless ref $self;

  _verbose("wait_one $self, $timeout");

  if (!defined($self->{SAFE_PID})) {
    # No pid => not running
    _verbose("No pid => not running");
    return 0;
  }

  if (defined $self->{EXIT_STATUS}) {
    # Exit status already set => not running
    _verbose("Exit status already set => not running");
    return 0;
  }

  my $pid = $self->{SAFE_PID};

  my $use_alarm;
  my $blocking;
  if (defined $timeout) {
    if ($timeout == 0) {
      # 0 -> just poll with WNOHANG
      $blocking  = 0;
      $use_alarm = 0;
    } else {
      # >0 -> wait blocking for max timeout seconds
      $blocking  = 1;
      $use_alarm = 1;
    }
  } else {
    # undef -> wait blocking infinitely
    $blocking  = 1;
    $use_alarm = 0;
  }

  my $retpid;
  eval {
    # alarm should break the wait
    local $SIG{ALRM} = sub { die "waitpid timeout"; };

    alarm($timeout) if $use_alarm;

    $retpid = waitpid($pid, $blocking ? 0 : &WNOHANG);

    alarm(0) if $use_alarm;
  };

  if ($@) {
    die "Got unexpected: $@" if ($@ !~ /waitpid timeout/);
    if (!defined $retpid) {
      # Got timeout
      _verbose("Got timeout");
      return 1;
    }
    # Got pid _and_ alarm, continue
    _verbose("Got pid and alarm, continue");
  }

  if ($retpid == 0) {
    # 0 => still running
    _verbose("0 => still running");
    return 1;
  }

  if (not $blocking and $retpid == -1) {
    # still running
    _verbose("still running");
    return 1;
  }

  $self->_collect();
  return 0;
}

# Wait for any process to exit. Returns a reference to the SafeProcess
# that exited or undefined.
sub wait_any {
  my $ret_pid;
  if (IS_WIN32PERL) {
    # Can't wait for -1 => use a polling loop
    do {
      Win32::Sleep(10);    # 10 milli seconds
      foreach my $pid (keys %running) {
        $ret_pid = waitpid($pid, &WNOHANG);
        last if $pid == $ret_pid;
      }
    } while ($ret_pid == 0);
  } else {
    $ret_pid = waitpid(-1, 0);
    if ($ret_pid <= 0) {
      # No more processes to wait for
      print STDERR "wait_any, got invalid pid: $ret_pid\n";
      return undef;
    }
  }

  # Look it up in "running" table
  my $proc = $running{$ret_pid};
  unless (defined $proc) {
    print STDERR "Could not find pid: $ret_pid in running list\n";
    print STDERR "running: " . join(", ", keys(%running)) . "\n";
    return undef;
  }
  $proc->_collect;
  return $proc;
}

# Wait for any process to exit, or a timeout. Returns a reference
# to the SafeProcess that exited or a pseudo-process with
# $proc->{timeout} == 1.
sub wait_any_timeout {
  my $class   = shift;
  my $timeout = shift;

  my $proc;
  my $millis = 10;
  do {
    ::mtr_milli_sleep($millis);
    # Slowly increse interval up to max. 1 second
    $millis++ if $millis < 1000;
    # Return a "fake" process for timeout
    if (::has_expired($timeout)) {
      $proc = bless({ SAFE_PID  => 0,
                      SAFE_NAME => "timer",
                      timeout   => 1,
                    },
                    $class);
    } else {
      $proc = check_any();
    }
  } while (!$proc);

  return $proc;
}

# Wait for all processes to exit
sub wait_all {
  while (keys %running) {
    wait_any();
  }
}

# Set global flag to tell all safe_process to exit after starting child
sub start_exit {
  $start_exit = 1;
}

# Check if any process has exited, but don't wait. Returns a reference
# to the SafeProcess that exited or undefined.
sub check_any {
  for my $proc (values %running) {
    if ($proc->is_child($$)) {
      if (not $proc->wait_one(0)) {
        _verbose("Found exited $proc");
        return $proc;
      }
    }
  }
  return undef;
}

# Overload string operator and fallback to default functions if no
# overloaded function is found.
use overload
  '""'     => \&self2str,
  fallback => 1;

# Return the process as a nicely formatted string
sub self2str {
  my ($self)      = @_;
  my $exit_status = $self->{EXIT_STATUS};
  my $name        = $self->{SAFE_NAME};
  my $pid         = $self->{SAFE_PID};
  my $winpid      = $self->{SAFE_WINPID};

  my $str = "[$name - pid: $pid";
  $str = $str . ", winpid: $winpid"    if defined $winpid;
  $str = $str . ", exit: $exit_status" if defined $exit_status;
  $str = $str . "]";
}

sub _verbose {
  return unless $_verbose;
  print STDERR " ## " . @_ . "\n";
}

sub pid {
  my ($self) = @_;
  return $self->{SAFE_PID};
}

sub user_data {
  my ($self) = @_;
  return $self->{SAFE_USER_DATA};
}

1;
