# -*- cperl -*-
# Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.
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

package My::Exec;

use strict;
use warnings;
use Carp;
use File::Basename;
use IO::File;

use base qw(Exporter);
our @EXPORT = qw(exec_print_on_error);

# Generate a logfile name from a command
sub get_logfile_name {
  my $cmd = shift;

  # Get logfile name
  my @cmd_parts = split(' ', $cmd);
  my $logfile_base = fileparse($cmd_parts[0]);
  $logfile_base =~ s/[^a-zA-Z0-9_]*//g;
  my $logfile_name = "";
  my $log_dir      = $ENV{MYSQLTEST_VARDIR};

  for my $i (1 .. 100) {
    my $proposed_logfile_name = "$log_dir/$logfile_base" . '_' . $i . ".log";
    if (!-f $proposed_logfile_name) {
      # We can use this file name
      $logfile_name = $proposed_logfile_name;
      last;
    }
  }

  return $logfile_name;
}

# Show the last "max_lines" from file
sub show_last_lines_from_file {
  my ($filename, $max_lines) = @_;

  my $F = IO::File->new($filename, "r") or
    print "Failed to open file '$filename' for reading: $!\n" and
    return;

  my @input = <$F>;
  my $lines = scalar(@input);
  $lines = $max_lines if $lines > $max_lines;
  print "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
  print "Last '$lines' lines of output from command:\n";

  foreach my $line (splice(@input, -$lines)) {
    print $line;
  }

  print "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
  $F->close();
}

# Executes command, and prints n last lines of output from the command
# only if the command fails. If the command runs successfully, no output
# is written.
#
# Parameters:
#   cmd       - the command to run
#   max_lines - the maximum number of lines of output to show on error
#               (default 200).
#
# Returns:
#   1 on success
#   0 on error.
#
# Example:
#   use My::Exec;
#   my $res = exec_print_on_error("./mtr --suite=ndb ndb_dd_varsize");
sub exec_print_on_error {
  my $cmd = shift;
  my $max_lines = shift || 200;

  my $logfile_name  = get_logfile_name($cmd);
  # Redirect stdout and stderr of command to log file
  $cmd = $cmd . " > $logfile_name 2>&1";

  # Execute command
  print "Running command\n";
  system($cmd);
  print "Result of command: $?\n";

  if ($? == 0) {
    # Test program suceeded
    return 1;
  }

  # Test program failed
  if ($? == -1) {
    # Failed to execute program
    print "Failed to execute '$cmd': $!\n";
  } elsif ($?) {
    # Test program failed
    my $sig    = $? & 127;
    my $return = $? >> 8;
    print "Command that failed: '$cmd'\n";
    print "Test program killed by signal $sig\n"     if $sig;
    print "Test program failed with error $return\n" if $return;

    # Show the "max_lines" last lines from the log file
    show_last_lines_from_file($logfile_name, $max_lines);
  }
  return 0;
}
