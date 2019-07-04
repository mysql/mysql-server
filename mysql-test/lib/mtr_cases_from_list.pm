# -*- cperl -*-
# Copyright (c) 2005, 2019, Oracle and/or its affiliates. All rights reserved.
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
#
# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

package mtr_cases_from_list;

use strict;

use File::Basename;

use My::File::Path qw / get_bld_path /;
use My::Platform;

use base qw(Exporter);
our @EXPORT = qw(collect_test_cases_from_list);

# Collect information about test cases to be run
sub collect_test_cases_from_list ($$$) {
  my $opt_cases        = shift;
  my $opt_do_test_list = shift;
  my $opt_ctest        = shift;

  $opt_do_test_list =~ s/^\~\//$ENV{HOME}\// if ($opt_do_test_list ne "");
  $opt_do_test_list = get_bld_path($opt_do_test_list);

  open(FILE, "<", $opt_do_test_list) or
    die("Error: Cannot open '$opt_do_test_list' file.");

  my @tests = <FILE>;
  close FILE;
  chomp(@tests);

  foreach my $test (@tests) {
    # Skip comments
    next if ($test =~ /^[\s ]*#/);

    # Skip an empty line
    next if ($test =~ /^\s*$/);

    # Remove prepreceding or trailing spaces if any
    $test =~ s/^\s*|\s*$//g;

    # Replace '\' with '/' on windows
    $test =~ s/\\/\//g if IS_WINDOWS;

    my $test_dir = dirname($test);
    if ($test_dir eq '.') {
      # Push the test case to '@opt_cases'
      push(@$opt_cases, $test);
    } else {
      my $abs_path = $test;

      # Check if path value specified is a relative path
      if (!File::Spec->file_name_is_absolute($test)) {
        # Append basedir path to the test file path
        $abs_path = "$::basedir/$test";
      }

      # Check if path to test file exists
      die("Test file '$test' doesn't exist.") if (!-e $abs_path);

      # Check if test file is inside the base directory
      die("Test file '$abs_path' is not inside the base directory " .
          "'$::basedir'.")
        if ($abs_path !~ /^$::basedir/);

      # Check whether test file name ends with '.test' extension
      die("Invalid test file '$test'.") if ($test !~ /\.test$/);

      my $sname = find_suite_name($test_dir);
      die("Invalid path '$test' to the test file.") if !$sname;
      my $tname = basename($test);

      # Push suite_name.test_name to '@opt_cases'
      push(@$opt_cases, $sname . "." . $tname);
    }
  }

  if (@$opt_cases == 0) {
    die("Error: Test list doesn't contain test cases.");
  }

  # To avoid execution of unit tests.
  $$opt_ctest = 0;
}

sub find_suite_name($) {
  my $test_dir = shift;

  my @paths = ("internal/cloud/mysql-test/suite/(.*)/t",
               "internal/mysql-test/suite/(.*)/t",
               "internal/plugin/(.*)/tests/mtr/t",
               "mysql-test/suite/(.*)/t",
               "mysql-test/suite/(.*)"                     # federated suite
  );

  my $pattern = join("|", @paths);
  return $+ if ($test_dir =~ /$pattern/);

  # Check for main suite
  if ($test_dir =~ /mysql-test\/t$/) {
    return "main";
  }
}

1;
