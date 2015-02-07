# -*- cperl -*-
# Copyright (c) 2007, 2015 Oracle and/or its affiliates. All rights reserved.
# Use is subject to license terms.
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

use strict;

package My::SafeProcess::Base;

#
# Utility functions for Process management
#

use Carp;
use IO::Pipe;

use base qw(Exporter);
our @EXPORT= qw(create_process);



#
# safe_fork
#   Retry a couple of times if fork returns EAGAIN
#
sub _safe_fork {
  my $retries= 5;
  my $pid;

 FORK:
  {
    $pid= fork;
    if ( not defined($pid)) {

      croak("fork failed after: $!") if (!$retries--);

      warn("fork failed sleep 1 second and redo: $!");
      sleep(1);
      redo FORK;
    }
  }

  return $pid;
};


#
# Decode exit status
#
sub exit_status {
  my $self= shift;
  my $raw= $self->{EXIT_STATUS};

  croak("Can't call exit_status before process has died")
    unless defined $raw;

  if ($raw & 127)
  {
    # Killed by signal
    my $signal_num=  $raw & 127;
    my $dumped_core= $raw & 128;
    return 1; # Return error code
  }
  else
  {
    # Normal process exit
    return $raw >> 8;
  };
}

# threads.pm may not exist everywhere, so use only on Windows.

use if $^O eq "MSWin32", "threads";
use if $^O eq "MSWin32", "threads::shared";

my $win32_spawn_lock :shared;


#
# Create a new process
# Return pid of the new process
#
sub create_process {
  my %opts=
    (
     @_
    );

  my $path     = delete($opts{'path'})    or die "path required";
  my $args     = delete($opts{'args'})    or die "args required";
  my $input    = delete($opts{'input'});
  my $output   = delete($opts{'output'});
  my $error    = delete($opts{'error'});
  my $pid_file = delete($opts{'pid_file'});
  my $open_mode= $opts{append} ? ">>" : ">";

  if ($^O eq "MSWin32"){

    lock($win32_spawn_lock);

    #printf STDERR "stdin %d, stdout %d, stderr %d\n",
    #    fileno STDIN, fileno STDOUT, fileno STDERR;

    # input output redirect
    my ($oldin, $oldout, $olderr);
    open $oldin,  '<&', \*STDIN  or die "Failed to save old stdin: $!";
    open $oldout, '>&', \*STDOUT or die "Failed to save old stdout: $!";
    open $olderr, '>&', \*STDERR or die "Failed to save old stderr: $!";

    if ( $input ) {
      if ( ! open(STDIN, "<", $input) ) {
	croak("can't redirect STDIN to '$input': $!");
      }
    }

    if ( $output ) {
      if ( ! open(STDOUT, $open_mode, $output) ) {
	croak("can't redirect STDOUT to '$output': $!");
      }
    }

    if ( $error ) {
      if ( $output eq $error ) {
	if ( ! open(STDERR, ">&STDOUT") ) {
	  croak("can't dup STDOUT: $!");
	}
      }
      elsif ( ! open(STDERR, $open_mode, $error) ) {
	croak("can't redirect STDERR to '$error': $!");
      }
    }


    # Magic use of 'system(1, @args)' to spawn a process
    # and get a proper Win32 pid
    unshift (@$args, $path);
    my $pid= system(1, @$args);
    if ( $pid == 0 ){
      print $olderr "create_process failed: $^E\n";
      die "create_process failed: $^E";
    }

    # Retore IO redirects
    open STDERR, '>&', $olderr
      or croak("unable to reestablish STDERR");
    open STDOUT, '>&', $oldout
      or croak("unable to reestablish STDOUT");
    open STDIN, '<&', $oldin
      or croak("unable to reestablish STDIN");
    #printf STDERR "stdin %d, stdout %d, stderr %d\n",
    #    fileno STDIN, fileno STDOUT, fileno STDERR;
    return $pid;

  }

  local $SIG{PIPE}= sub { print STDERR "Got signal $@\n"; };
  my $pipe= IO::Pipe->new();
  my $pid= _safe_fork();
  if ($pid){
    # Parent
    $pipe->reader();
    my $line= <$pipe>; # Wait for child to say it's ready
    # if pid-file is defined, read process id from pid-file.
    if ( defined $pid_file ) {
      sleep 1 until -e $pid_file;
      open FILE, $pid_file;
      chomp(my $pid_val = <FILE>);
      close FILE;
      return $pid_val
    }
    return $pid;
  }

  $SIG{INT}= 'DEFAULT';

  # Make this process it's own process group to be able to kill
  # it and any childs(that hasn't changed group themself)
  setpgrp(0,0) if $opts{setpgrp};

  if ( $output and !open(STDOUT, $open_mode, $output) ) {
    croak("can't redirect STDOUT to '$output': $!");
  }

  if ( $error ) {
    if ( defined $output and $output eq $error ) {
      if ( ! open(STDERR, ">&STDOUT") ) {
	croak("can't dup STDOUT: $!");
      }
    }
    elsif ( ! open(STDERR, $open_mode, $error) ) {
      croak("can't redirect STDERR to '$error': $!");
    }
  }

  if ( $input ) {
    if ( ! open(STDIN, "<", $input) ) {
      croak("can't redirect STDIN to '$input': $!");
    }
  }

  if ( !exec($path, @$args) ){
    croak("Failed to exec '$path': $!");
  }
  # Tell parent to continue
  $pipe->writer();
  print $pipe "ready\n";

  croak("Should never come here");

}

1;

