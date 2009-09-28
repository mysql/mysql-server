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

package My::Platform;

use strict;
use File::Basename;
use File::Path;

use base qw(Exporter);
our @EXPORT= qw(IS_CYGWIN IS_WINDOWS IS_WIN32PERL
		native_path posix_path mixed_path
                check_socket_path_length process_alive);

BEGIN {
  if ($^O eq "cygwin") {
    # Make sure cygpath works
    if ((system("cygpath > /dev/null 2>&1") >> 8) != 1){
      die "Could not execute 'cygpath': $!";
    }
    eval 'sub IS_CYGWIN { 1 }';
  }
  else {
    eval 'sub IS_CYGWIN { 0 }';
  }
  if ($^O eq "MSWin32") {
    eval 'sub IS_WIN32PERL { 1 }';
  }
  else {
    eval 'sub IS_WIN32PERL { 0 }';
  }
}

BEGIN {
  if (IS_CYGWIN or IS_WIN32PERL) {
    eval 'sub IS_WINDOWS { 1 }';
  }
  else {
    eval 'sub IS_WINDOWS { 0 }';
  }
}


#
# native_path
# Convert from path format used by perl to the underlying
# operating systems format
#
# NOTE
#  Used when running windows binaries (that expect windows paths)
#  in cygwin perl (that uses unix paths)
#

use Memoize;
if (!IS_WIN32PERL){
  memoize('mixed_path');
  memoize('native_path');
  memoize('posix_path');
}

sub mixed_path {
  my ($path)= @_;
  if (IS_CYGWIN){
    return unless defined $path;
    my $cmd= "cygpath -m $path";
    $path= `$cmd` or
      print "Failed to run: '$cmd', $!\n";
    chomp $path;
  }
  return $path;
}

sub native_path {
  my ($path)= @_;
  $path=~ s/\//\\/g
    if (IS_CYGWIN or IS_WIN32PERL);
  return $path;
}

sub posix_path {
  my ($path)= @_;
  if (IS_CYGWIN){
    return unless defined $path;
    $path= `cygpath $path`;
    chomp $path;
  }
  return $path;
}

use File::Temp qw /tempdir/;

sub check_socket_path_length {
  my ($path)= @_;

  return 0 if IS_WINDOWS;
  # This may not be true, but we can't test for it on AIX due to Perl bug
  # See Bug #45771
  return 0 if ($^O eq 'aix');

  require IO::Socket::UNIX;

  my $truncated= undef;

  # Create a tempfile name with same length as "path"
  my $tmpdir = tempdir( CLEANUP => 0);
  my $len = length($path) - length($tmpdir) - 1;
  my $testfile = $tmpdir . "/" . "x" x ($len  > 0 ? $len : 1);
  my $sock;
  eval {
    $sock= new IO::Socket::UNIX
      (
       Local => $testfile,
       Listen => 1,
      );
    $truncated= 1; # Be negatvie

    die "Could not create UNIX domain socket: $!"
      unless defined $sock;

    die "UNIX domain socket path was truncated"
      unless ($testfile eq $sock->hostpath());

    $truncated= 0; # Yes, it worked!

  };

  die "Unexpected failure when checking socket path length: $@"
    if $@ and not defined $truncated;

  $sock= undef;  # Close socket
  rmtree($tmpdir); # Remove the tempdir and any socket file created
  return $truncated;
}


sub process_alive {
  my ($pid)= @_;
  die "usage: process_alive(pid)" unless $pid;

  return kill(0, $pid) unless IS_WINDOWS;

  my @list= split(/,/, `tasklist /FI "PID eq $pid" /NH /FO CSV`);
  my $ret_pid= eval($list[1]);
  return ($ret_pid == $pid);
}


1;
