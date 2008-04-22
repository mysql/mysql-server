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

use base qw(Exporter);
our @EXPORT= qw(IS_CYGWIN IS_WINDOWS IS_WIN32PERL
		native_path posix_path mixed_path
                check_socket_path_length);

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

sub mixed_path {
  my ($path)= @_;
  if (IS_CYGWIN){
    return unless defined $path;
    $path= `cygpath -m $path`;
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


sub check_socket_path_length {
  my ($path)= @_;
  my $truncated= 0;

  return 0 if IS_WINDOWS;

  require IO::Socket::UNIX;

  my $sock = new IO::Socket::UNIX
  (
   Local => $path,
   Listen => 1,
  );
  if (!defined $sock){
    # Could not create a UNIX domain socket
    return 0; # Ok, will not be used by mysqld either    
  }
  if ($path ne $sock->hostpath()){
    # Path was truncated
    $truncated= 1;
    # Output diagnostic messages
    print "path: '$path', length: ", length($path) ,"\n";
    print "hostpath: '", $sock->hostpath(),
	  "', length: ", length($sock->hostpath()), "\n";
  }
  $sock= undef; # Close socket
  unlink($path); # Remove the physical file
  return $truncated;
}


1;
