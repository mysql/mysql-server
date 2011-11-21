# -*- cperl -*-
# Copyright (c) 2004-2006 MySQL AB, 2008 Sun Microsystems, Inc.
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
use File::Find;

sub mtr_native_path($);
sub mtr_init_args ($);
sub mtr_add_arg ($$@);
sub mtr_path_exists(@);
sub mtr_script_exists(@);
sub mtr_file_exists(@);
sub mtr_exe_exists(@);
sub mtr_exe_maybe_exists(@);
sub mtr_copy_dir($$);
sub mtr_rmtree($);
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
    if ($::glob_win32);
  return $path;
}


# FIXME move to own lib

sub mtr_init_args ($) {
  my $args = shift;
  $$args = [];                            # Empty list
}

sub mtr_add_arg ($$@) {
  my $args=   shift;
  my $format= shift;
  my @fargs = @_;

  push(@$args, sprintf($format, @fargs));
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
    if($::glob_win32)
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

  map {$_.= ".exe"} @path if $::glob_win32;
  map {$_.= ".nlm"} @path if $::glob_netware;
  foreach my $path ( @path )
  {
    if($::glob_win32)
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


sub mtr_copy_dir($$) {
  my $from_dir= shift;
  my $to_dir= shift;

  # mtr_verbose("Copying from $from_dir to $to_dir");

  mkpath("$to_dir");
  opendir(DIR, "$from_dir")
    or mtr_error("Can't find $from_dir$!");
  for(readdir(DIR)) {
    next if "$_" eq "." or "$_" eq "..";
    if ( -d "$from_dir/$_" )
    {
      mtr_copy_dir("$from_dir/$_", "$to_dir/$_");
      next;
    }
    copy("$from_dir/$_", "$to_dir/$_");
  }
  closedir(DIR);

}


sub mtr_rmtree($) {
  my ($dir)= @_;
  mtr_verbose("mtr_rmtree: $dir");

  # Try to use File::Path::rmtree. Recent versions
  # handles removal of directories and files that don't
  # have full permissions, while older versions
  # may have a problem with that and we use our own version

  eval { rmtree($dir); };
  if ( $@ ) {
    mtr_warning("rmtree($dir) failed, trying with File::Find...");

    my $errors= 0;

    # chmod
    find( {
	   no_chdir => 1,
	   wanted => sub {
	     chmod(0777, $_)
	       or mtr_warning("couldn't chmod(0777, $_): $!") and $errors++;
	   }
	  },
	  $dir
	);

    # rm
    finddepth( {
	   no_chdir => 1,
	   wanted => sub {
	     my $file= $_;
	     # Use special underscore (_) filehandle, caches stat info
	     if (!-l $file and -d _ ) {
	       rmdir($file) or
		 mtr_warning("couldn't rmdir($file): $!") and $errors++;
	     } else {
	       unlink($file)
		 or mtr_warning("couldn't unlink($file): $!") and $errors++;
	     }
	   }
	  },
	  $dir
	);

    mtr_error("Failed to remove '$dir'") if $errors;

    mtr_report("OK, that worked!");
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

#
# Compare two arrays and put all unequal elements into a new one
#
sub mtr_diff_opts ($$) {
  my $l1= shift;
  my $l2= shift;
  my $f;
  my $l= [];
  foreach my $e1 (@$l1) 
  {    
    $f= undef;
    foreach my $e2 (@$l2) 
    {
      $f= 1 unless ($e1 ne $e2);
    }
    push(@$l, $e1) unless (defined $f);
  }
  foreach my $e2 (@$l2) 
  {
    $f= undef;
    foreach my $e1 (@$l1) 
    {
      $f= 1 unless ($e1 ne $e2);
    }
    push(@$l, $e2) unless (defined $f);
  }
  return $l;
}

1;
