# -*- cperl -*-
# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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

package My::Handles;


use strict;
use Carp;

use My::Platform;

my $handle_exe;


if (IS_WINDOWS){
  # Check if handle.exe is available
  # Pass switch to accept the EULA to avoid hanging
  # if the program hasn't been run before.
  my $list= `handle.exe -? -accepteula 2>&1`;
  foreach my $line (split('\n', $list))
  {
    $handle_exe= "$1.$2"
      if ($line =~ /Handle v([0-9]*)\.([0-9]*)/);
  }
  if ($handle_exe){
    print "Found handle.exe version $handle_exe\n";
  }
}


sub show_handles
{
  my ($dir)= @_;
  return unless $handle_exe;
  return unless $dir;

  $dir= native_path($dir);

  # Get a list of open handles in a particular directory
  my $list= `handle.exe "$dir" 2>&1` or return;

  foreach my $line (split('\n', $list))
  {
    return if ($line =~ /No matching handles found/);
  }

  print "\n";
  print "=" x 50, "\n";
  print "Open handles in '$dir':\n";
  print "$list\n";
  print "=" x 50, "\n\n";

  return;
}

1;
