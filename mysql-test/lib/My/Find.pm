# -*- cperl -*-
# Copyright (c) 2007, 2011, Oracle and/or its affiliates.
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


package My::Find;

#
# Utility functions to find files in a MySQL source or bindist
#

use strict;
use Carp;
use My::Platform;

use base qw(Exporter);
our @EXPORT= qw(my_find_bin my_find_dir my_find_file NOT_REQUIRED);

my $bin_extension= ".exe" if IS_WINDOWS;

# Helper function to be used for fourth parameter to find functions
sub NOT_REQUIRED { return 0; }

#
# my_find_bin - find an executable with "name_1...name_n" in
# paths "path_1...path_n" and return the full path
#
# Example:
#    my $mysqld_exe= my_find_bin($basedir.
#                                ["sql", "bin"],
#                                ["mysqld", "mysqld-debug"]);
#    my $mysql_exe= my_find_bin($basedir,
#                               ["client", "bin"],
#                               "mysql");
#
#
#    To check if something exists, use the required parameter
#    set to 0, the function will return an empty string if the
#    binary is not found
#    my $mysql_exe= my_find_bin($basedir,
#                               ["client", "bin"],
#                               "mysql", NOT_REQUIRED);
#
# NOTE: The function honours MTR_VS_CONFIG environment variable
#
#
sub my_find_bin {
  my ($base, $paths, $names, $required)= @_;
  croak "usage: my_find_bin(<base>, <paths>, <names>, [<required>])"
    unless @_ == 4 or @_ == 3;

  # -------------------------------------------------------
  # Find and return the first executable
  # -------------------------------------------------------
  foreach my $path (my_build_path_list($base, $paths, $names, $bin_extension)) {
    return $path if ( -x $path or (IS_WINDOWS and -f $path) );
  }
  if (defined $required and $required == NOT_REQUIRED){
    # Return empty string to indicate not found
    return "";
  }
  find_error($base, $paths, $names);
}


#
# my_find_file - find a file with "name_1...name_n" in
# paths "path_1...path_n" and return the full path
#
# Example:
#    my $mysqld_exe= my_find_file($basedir.
#                                ["sql", "bin"],
#                                "filename");
#
#
# Also supports NOT_REQUIRED flag
#
# NOTE: The function honours MTR_VS_CONFIG environment variable
#
#
sub my_find_file {
  my ($base, $paths, $names, $required)= @_;
  croak "usage: my_find_file(<base>, <paths>, <names>, [<required>])"
    unless @_ == 4 or @_ == 3;

  # -------------------------------------------------------
  # Find and return the first executable
  # -------------------------------------------------------
  foreach my $path (my_build_path_list($base, $paths, $names, $bin_extension)) {
    return $path if ( -f $path );
  }
  if (defined $required and $required == NOT_REQUIRED){
    # Return empty string to indicate not found
    return "";
  }
  find_error($base, $paths, $names);
}


#
# my_find_dir - find the existing directories in one of
# the given paths. Returns the first found in the scalar context
# and all of them in the list context.
#
# Example:
#    my $charset_set= my_find_dir($basedir,
#                                 ["mysql/share","sql/share", "share"],
#                                 ["charset"]);
# or
#    my $charset_set= my_find_dir($basedir,
#                                 ['client_release', 'client_debug',
#			           'client', 'bin']);
#
# NOTE: The function honours MTR_VS_CONFIG environment variable
#
#
sub my_find_dir {
  my ($base, $paths, $dirs, $required)= @_;
  croak "usage: my_find_dir(<base>, <paths>[, <dirs>[, <required>]])"
    unless (@_ >= 2 and @_ <= 4);

  my @all;
  foreach my $path (my_build_path_list($base, $paths, $dirs)) {
    next unless -d $path;
    return $path unless wantarray;
    push @all, $path;
  }
  return @all if @all;
  return wantarray ? () : "" if defined $required and $required == NOT_REQUIRED;
  find_error($base, $paths, $dirs);
}


sub my_build_path_list {
  my ($base, $paths, $names, $extension)= @_;

  # Convert the arguments into two normal arrays to ease
  # further mappings
  my (@names, @paths);
  push(@names, ref $names eq "ARRAY" ? @$names : $names);
  push(@paths, ref $paths eq "ARRAY" ? @$paths : $paths);

  #print "base: $base\n";
  #print "names: @names\n";
  #print "paths: @paths\n";

  # User can select to look in a special build dir
  # which is a subdirectory of any of the paths
  my @extra_dirs;
  my $build_dir= $::opt_vs_config || $ENV{MTR_VS_CONFIG} || $ENV{MTR_BUILD_DIR};
  push(@extra_dirs, $build_dir) if defined $build_dir;

  if (defined $extension){
    # Append extension to names, if name does not already have extension
    map { $_.=$extension unless /\.(.*)+$/ } @names;
  }

  # -------------------------------------------------------
  # CMake generator specific (Visual Studio and Xcode have multimode builds)
  # -------------------------------------------------------

  # Add the default extra build dirs unless a specific one has
  # already been selected
  push(@extra_dirs,
   ("Release",
    "Relwithdebinfo",
    "Debug")) if @extra_dirs == 0;


  #print "extra_build_dir: @extra_dirs\n";

  # -------------------------------------------------------
  # Build cross product of "paths * extra_build_dirs"
  # -------------------------------------------------------
  push(@paths, map { my $path= $_;
		     map  { "$path/$_" } @extra_dirs
		   } @paths);
  #print "paths: @paths\n";

  # -------------------------------------------------------
  # Build cross product of "paths * names"
  # -------------------------------------------------------
  @paths= map { my $path= $_;
		map  { "$path/$_" } @names
	      } @paths;
  #print "paths: @paths\n";

  # -------------------------------------------------------
  # Prepend base to all paths
  # -------------------------------------------------------
  @paths= map { "$base/$_" } @paths;
  #print "paths: @paths\n";

  # -------------------------------------------------------
  # Glob all paths to expand wildcards
  # -------------------------------------------------------
  @paths= map { glob("$_") } @paths;
  #print "paths: @paths\n";

  # -------------------------------------------------------
  # Return the list of paths
  # -------------------------------------------------------
  return @paths;
}


sub commify {
  return
    (@_ == 0) ? '' :
      (@_ == 1) ? $_[0] :
	(@_ == 2) ? join(" or ", @_) :
	  join(", ", @_[0..($#_-1)], "or $_[-1]");

}


sub fnuttify {
  return map('\''.$_.'\'', @_);
}


sub find_error {
  my ($base, $paths, $names)= @_;

  my (@names, @paths);
  push(@names, ref $names eq "ARRAY" ? @$names : $names);
  push(@paths, ref $paths eq "ARRAY" ? @$paths : $paths);

  croak "** ERROR: Could not find ",
    commify(fnuttify(@names)), " in ",
      commify(fnuttify(my_build_path_list($base, $paths, $names))), "\n";
}

1;
