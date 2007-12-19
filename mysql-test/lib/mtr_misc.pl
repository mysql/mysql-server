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

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub mtr_native_path($);
sub mtr_init_args ($);
sub mtr_add_arg ($$@);
sub mtr_args2str($@);
sub mtr_path_exists(@);
sub mtr_script_exists(@);
sub mtr_file_exists(@);
sub mtr_exe_exists(@);
sub mtr_exe_maybe_exists(@);
sub mtr_same_opts($$);
sub mtr_cmp_opts($$);

##############################################################################
#
#  Misc
#
##############################################################################

# Convert path to OS native format
sub mtr_native_path($)
{
  my $path= shift;

  # MySQL version before 5.0 still use cygwin, no need
  # to convert path
  return $path
    if ($::mysql_version_id < 50000);

  $path=~ s/\//\\/g
    if ($::is_win32);
  return $path;
}


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
    if ($::is_win32 and grep(/\s/, @fargs));

  push(@$args, sprintf($format, @fargs));
}

sub mtr_args2str($@) {
  my $exe=   shift or die;
  return join(" ", mtr_native_path($exe), @_);
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
    if($::is_win32)
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

  map {$_.= ".exe"} @path if $::is_win32;
  foreach my $path ( @path )
  {
    if($::is_win32)
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



sub mtr_same_opts ($$) {
  my $l1= shift;
  my $l2= shift;
  return mtr_cmp_opts($l1,$l2) == 0;
}

sub mtr_cmp_opts ($$) {
  my $l1= shift;
  my $l2= shift;

  my @l1= @$l1;
  my @l2= @$l2;

  return -1 if @l1 < @l2;
  return  1 if @l1 > @l2;

  while ( @l1 )                         # Same length
  {
    my $e1= shift @l1;
    my $e2= shift @l2;
    my $cmp= ($e1 cmp $e2);
    return $cmp if $cmp != 0;
  }

  return 0;                             # They are the same
}


sub mtr_milli_sleep {
  die "usage: mtr_milli_sleep(milliseconds)" unless @_ == 1;
  my ($millis)= @_;

  select(undef, undef, undef, ($millis/1000));
}

#
# Compare two arrays and put all unequal elements into a new one
#
sub mtr_diff_opts ($$) {
  my $l1= shift;
  my $l2= shift;
  my $found;
  my @result;
  foreach my $e1 (@$l1)
  {
    $found= undef;
    foreach my $e2 (@$l2)
    {
      $found= 1 unless ($e1 ne $e2);
    }
    push(@result, $e1) unless (defined $found);
  }
  foreach my $e2 (@$l2)
  {
    $found= undef;
    foreach my $e1 (@$l1)
    {
      $found= 1 unless ($e1 ne $e2);
    }
    push(@result, $e2) unless (defined $found);
  }
  return @result;
}

1;
