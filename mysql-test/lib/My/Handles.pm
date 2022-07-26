# -*- cperl -*-
# Copyright (c) 2008, 2022, Oracle and/or its affiliates.
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

package My::Handles;

use strict;
use Carp;

use My::Platform;

my $handle_exe;

if (IS_WINDOWS) {
  # Check if handle.exe is available. Pass switch to accept the EULA
  # to avoid hanging if the program hasn't been run before.
  my $list = `handle.exe -? -accepteula 2>&1`;
  foreach my $line (split('\n', $list)) {
    $handle_exe = "$1.$2"
      if ($line =~ /Handle v([0-9]*)\.([0-9]*)/);
  }

  if ($handle_exe) {
    print "Found handle.exe version $handle_exe\n";
  }
}

sub show_handles {
  my ($dir) = @_;
  return unless $handle_exe;
  return unless $dir;

  $dir = native_path($dir);

  # Get a list of open handles in a particular directory
  my $list = `handle.exe "$dir" 2>&1` or return;

  foreach my $line (split('\n', $list)) {
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
