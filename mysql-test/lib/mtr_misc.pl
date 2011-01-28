# -*- cperl -*-
# Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

use My::Platform;

sub mtr_init_args ($);
sub mtr_add_arg ($$@);
sub mtr_args2str($@);
sub mtr_path_exists(@);
sub mtr_script_exists(@);
sub mtr_file_exists(@);
sub mtr_exe_exists(@);
sub mtr_exe_maybe_exists(@);
sub mtr_milli_sleep($);
sub start_timer($);
sub has_expired($);

##############################################################################
#
#  Args
#
##############################################################################

sub mtr_init_args ($) {
  my $args = shift;
  $$args = [];                            # Empty list
}

sub mtr_add_arg ($$@) {
  my $args=   shift;
  my $format= shift;
  my @fargs = @_;

  # Quote args if args contain space
  $format= "\"$format\""
    if (IS_WINDOWS and grep(/\s/, @fargs));

  push(@$args, sprintf($format, @fargs));
}

sub mtr_args2str($@) {
  my $exe=   shift or die;
  return join(" ", native_path($exe), @_);
}

##############################################################################

#
# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
#
sub mtr_path_exists (@) {
  foreach my $path ( @_ )
  {
    return $path if -e $path;
  }
  if ( @_ == 1 )
  {
    mtr_error("Could not find $_[0]");
  }
  else
  {
    mtr_error("Could not find any of " . join(" ", @_));
  }
}


#
# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
#
sub mtr_script_exists (@) {
  foreach my $path ( @_ )
  {
    if(IS_WINDOWS)
    {
      return $path if -f $path;
    }
    else
    {
      return $path if -x $path;
    }
  }
  if ( @_ == 1 )
  {
    mtr_error("Could not find $_[0]");
  }
  else
  {
    mtr_error("Could not find any of " . join(" ", @_));
  }
}


#
# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
#
sub mtr_file_exists (@) {
  foreach my $path ( @_ )
  {
    return $path if -e $path;
  }
  return "";
}


#
# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
#
sub mtr_exe_maybe_exists (@) {
  my @path= @_;

  map {$_.= ".exe"} @path if IS_WINDOWS;
  foreach my $path ( @path )
  {
    if(IS_WINDOWS)
    {
      return $path if -f $path;
    }
    else
    {
      return $path if -x $path;
    }
  }
  return "";
}


#
# NOTE! More specific paths should be given before less specific.
#
sub mtr_pl_maybe_exists (@) {
  my @path= @_;

  map {$_.= ".pl"} @path if IS_WINDOWS;
  foreach my $path ( @path )
  {
    if(IS_WINDOWS)
    {
      return $path if -f $path;
    }
    else
    {
      return $path if -x $path;
    }
  }
  return "";
}


#
# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
#
sub mtr_exe_exists (@) {
  my @path= @_;
  if (my $path= mtr_exe_maybe_exists(@path))
  {
    return $path;
  }
  # Could not find exe, show error
  if ( @path == 1 )
  {
    mtr_error("Could not find $path[0]");
  }
  else
  {
    mtr_error("Could not find any of " . join(" ", @path));
  }
}


sub mtr_milli_sleep ($) {
  die "usage: mtr_milli_sleep(milliseconds)" unless @_ == 1;
  my ($millis)= @_;

  select(undef, undef, undef, ($millis/1000));
}

# Simple functions to start and check timers (have to be actively polled)
# Timer can be "killed" by setting it to 0

sub start_timer ($) { return time + $_[0]; }

sub has_expired ($) { return $_[0] && time gt $_[0]; }

1;
