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


package My::Find;

#
# Utility functions to find files in a MySQL source or bindist
#

use strict;

use base qw(Exporter);
our @EXPORT= qw(my_find_bin my_find_dir);

our $vs_config_dir;

my $is_win= ($^O eq "MSWin32" or $^O eq "Win32");

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
# NOTE: The function honours MTR_VS_CONFIG environment variable
#
#
sub my_find_bin {
  my ($base, $paths, $names)= @_;
  die "usage: my_find_bin(<base>, <paths>, <names>)"
    unless @_ == 3;

  # -------------------------------------------------------
  # Find and return the first executable
  # -------------------------------------------------------
  foreach my $path (my_find_paths($base, $paths, $names)) {
    return $path if ( -x $path or ($is_win and -f $path) );
  }
  find_error($base, $paths, $names);
}


#
# my_find_dir - find the first existing directory in one of
# the given paths
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
  my ($base, $paths, $dirs)= @_;
  die "usage: my_find_dir(<base>, <paths>[, <dirs>])"
    unless (@_ == 3 or @_ == 2);

  # -------------------------------------------------------
  # Find and return the first directory
  # -------------------------------------------------------
  foreach my $path (my_find_paths($base, $paths, $dirs)) {
    return $path if ( -d $path );
  }
  find_error($base, $paths, $dirs);
}


sub my_find_paths {
  my ($base, $paths, $names)= @_;

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
  my $build_dir= $vs_config_dir || $ENV{MTR_VS_CONFIG} || $ENV{MTR_BUILD_DIR};
  push(@extra_dirs, $build_dir) if defined $build_dir;

  # -------------------------------------------------------
  # Windows specific
  # -------------------------------------------------------
  if ($is_win) {
    # Append .exe to names, if name does not already have extension
    map { $_.=".exe" unless /\.(.*)+$/ } @names;

    # Add the default extra build dirs unless a specific one has
    # already been selected
    push(@extra_dirs,
	 ("release",
	  "relwithdebinfo",
	  "debug")) if @extra_dirs == 0;
  }

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
  # Return the list of paths
  # -------------------------------------------------------
  return @paths;
}


sub find_error {
  my ($base, $paths, $names)= @_;

  my (@names, @paths);
  push(@names, ref $names eq "ARRAY" ? @$names : $names);
  push(@paths, ref $paths eq "ARRAY" ? @$paths : $paths);

  die "Could not find ",
    join(", ", @names), " in ",
      join(", ", my_find_paths($base, $paths, $names));
}

1;
