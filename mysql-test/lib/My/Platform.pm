# -*- cperl -*-
# Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.
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

package My::Platform;

use strict;
use File::Basename;
use File::Path;
use File::Temp qw /tempdir/;

use base qw(Exporter);
our @EXPORT = qw(IS_CYGWIN IS_MAC IS_WINDOWS IS_WIN32PERL
  check_socket_path_length mixed_path native_path
  posix_path process_alive);

BEGIN {
  if ($^O eq "cygwin") {
    # Make sure cygpath works
    if ((system("cygpath > /dev/null 2>&1") >> 8) != 1) {
      die "Could not execute 'cygpath': $!";
    }
    eval 'sub IS_CYGWIN { 1 }';
  } else {
    eval 'sub IS_CYGWIN { 0 }';
  }

  if ($^O eq "MSWin32") {
    eval 'sub IS_WIN32PERL { 1 }';
  } else {
    eval 'sub IS_WIN32PERL { 0 }';
  }
}

BEGIN {
  if (IS_CYGWIN or IS_WIN32PERL) {
    eval 'sub IS_WINDOWS { 1 }';
  } else {
    eval 'sub IS_WINDOWS { 0 }';
  }
}

BEGIN {
  if ($^O eq "darwin") {
    eval 'sub IS_MAC { 1 }';
  } else {
    eval 'sub IS_MAC { 0 }';
  }
}

# Convert from path format used by perl to the underlying
# operating systems format.
#
# NOTE
#   Used when running windows binaries (that expect windows paths)
#   in cygwin perl (that uses unix paths).
use Memoize;

if (!IS_WIN32PERL) {
  memoize('mixed_path');
  memoize('native_path');
  memoize('posix_path');
}

sub mixed_path {
  my ($path) = @_;
  if (IS_CYGWIN) {
    return unless defined $path;
    my $cmd = "cygpath -m $path";
    $path = `$cmd` or
      print "Failed to run: '$cmd', $!\n";
    chomp $path;
  }
  return $path;
}

sub native_path {
  my ($path) = @_;
  $path =~ s/\//\\/g
    if (IS_CYGWIN or IS_WIN32PERL);
  return $path;
}

sub posix_path {
  my ($path) = @_;
  if (IS_CYGWIN) {
    return unless defined $path;
    $path = `cygpath $path`;
    chomp $path;
  }
  return $path;
}

sub check_socket_path_length {
  my ($path, $parallel) = @_;

  return 0 if IS_WINDOWS;

  require IO::Socket::UNIX;

  my $truncated = undef;

  # Append extra chars if --parallel because $opt_tmpdir will be longer
  if ($parallel > 9 || $parallel eq "auto") {
    $path = $path . "xxx";
  } elsif ($parallel > 1) {
    $path = $path . "xx";
  }

  # Create a tempfile name with same length as "path"
  my $tmpdir   = tempdir(CLEANUP => 0);
  my $len      = length($path) - length($tmpdir) - 1;
  my $testfile = $tmpdir . "/" . "x" x ($len > 0 ? $len : 1);

  my $sock;
  eval {
    $sock = new IO::Socket::UNIX(Local  => $testfile,
                                 Listen => 1,);

    # Be negative
    $truncated = 1;

    die "Could not create UNIX domain socket: $!"
      unless defined $sock;

    die "UNIX domain socket path was truncated"
      unless ($testfile eq $sock->hostpath());

    $truncated = 0;    # Yes, it worked!
  };

  die "Unexpected failure when checking socket path length: $@"
    if $@ and
    not defined $truncated;

  # Close the socket
  $sock = undef;

  # Remove the tempdir and any socket file created
  rmtree($tmpdir);

  return $truncated;
}

sub process_alive {
  my ($pid) = @_;
  die "usage: process_alive(pid)" unless $pid;
  return kill(0, $pid) unless IS_WINDOWS;

  my @list = split(/,/, `tasklist /FI "PID eq $pid" /NH /FO CSV`);
  my $ret_pid = eval($list[1]);
  return ($ret_pid == $pid);
}

1;
