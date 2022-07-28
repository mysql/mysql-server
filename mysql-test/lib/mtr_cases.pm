# -*- cperl -*-
# Copyright (c) 2005, 2022, Oracle and/or its affiliates.
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

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

package mtr_cases;

use strict;

use base qw(Exporter);
our @EXPORT = qw(collect_option collect_test_cases init_pattern
  $do_test $group_replication);

use File::Basename;
use File::Spec::Functions qw / splitdir /;
use IO::File;

use My::Config;
use My::File::Path qw / get_bld_path /;
use My::Find;
use My::Platform;
use My::SysInfo;
use My::Test;

use mtr_match;
use mtr_report;

require "mtr_misc.pl";

my $secondary_engine_support = eval 'use mtr_secondary_engine; 1';
my $threads_support          = eval 'use threads; 1';
my $threads_shared_support   = eval 'use threads::shared; 1';

# Precompiled regex's for tests to do or skip
my $do_test_reg;
my $skip_test_reg;

# Related to adding InnoDB plugin combinations
my $do_innodb_plugin;
my $lib_innodb_plugin;

# If "Quick collect", set to 1 once a test to run has been found.
my $some_test_found;

# Options used for the collect phase
our $binlog_format;
our $default_storage_engine;
our $defaults_extra_file;
our $defaults_file;
our $do_test;
our $enable_disabled;
our $print_testcases;
our $quick_collect;
our $skip_rpl;
our $skip_test;
our $start_from;

# Set to 1 if you want the tests to override default storage engine
# settings, and use MyISAM as default (temporary option used in
# connection with the change of default storage engine to InnoDB).
our $default_myisam = 0;

our $group_replication;

sub collect_option {
  my ($opt, $value) = @_;

  # Evaluate $opt as string to use "Getopt::Long::Callback legacy API"
  my $opt_name = "$opt";

  # Convert - to _ in option name
  $opt_name =~ s/-/_/g;
  no strict 'refs';
  ${$opt_name} = $value;
}

sub init_pattern {
  my ($from, $what) = @_;
  return undef unless defined $from;

  if ($from =~ /^[a-z0-9\.]*$/) {
    # Does not contain any regex (except '.' that we allow as
    # separator betwen suite and testname), make the pattern match
    # beginning of string.
    $from = "^$from";
    mtr_verbose("$what='$from'");
  }

  # Check that pattern is a valid regex
  eval { "" =~ /$from/; 1 } or
    mtr_error("Invalid regex '$from' passed to $what\nPerl says: $@");

  return $from;
}

## Report an error for an invalid disabled test entry in a disabled.def
## file.
##
## Arguments:
##   $line              An entry from a disabled.def file
##   $line_number       Line number
##   $disabled_def_file disabled.def file location
sub report_disabled_test_format_error($$$) {
  my $line              = shift;
  my $line_number       = shift;
  my $disabled_def_file = shift;

  mtr_error("The format of line '$line' at '$disabled_def_file:$line_number' " .
            "is incorrect. The format is '<suitename>.<testcasename> " .
            "[\@platform|\@!platform] : <BUG|WL>#<XXXX> [<comment>]'.");
}

## Check if the test name format in a disabled.def file is correct. The
## format is "suite_name.test_name". If the format is incorrect, throw an
## error and abort the test run.
##
## Arguments:
##   $test_name         Test name
##   $line              An entry from a disabled.def file
##   $line_number       Line number
##   $disabled_def_file disabled.def file location
sub validate_test_name($$$$) {
  my $test_name         = shift;
  my $line              = shift;
  my $line_number       = shift;
  my $disabled_def_file = shift;

  # Test name must be in "<suitename>.<testcasename>" format.
  my ($suite_name, $tname, $extension) = split_testname($test_name);
  if (not defined $suite_name || not defined $tname || defined $extension) {
    report_disabled_test_format_error($line, $line_number, $disabled_def_file);
  }

  # disabled.def should contain only non-ndb suite tests, and disabled_ndb.def
  # should contain only any of the ndb suite tests.
  my $fname = basename($disabled_def_file);
  if (($fname eq 'disabled.def' and $suite_name =~ /ndb/) or
      ($fname eq 'disabled_ndb.def' and $suite_name !~ /ndb/)) {
    mtr_error("'$disabled_def_file' shouldn't contain '$suite_name' " .
              "suite test(s).");
  }
}

## Check if the test name section in a disabled.def file is correct. The
## format is "suite_name.test_name [@platform|@!platform]". If the format
## is incorrect, throw an error and abort the test run.
##
## Arguments:
##   $test_name_part    Test name section in a disabled test entry
##   $line              An entry from a disabled.def file
##   $line_number       Line number
##   $disabled_def_file disabled.def file location
sub validate_test_name_part($$$$) {
  my $test_name_part    = shift;
  my $line              = shift;
  my $line_number       = shift;
  my $disabled_def_file = shift;

  my $test_name;
  if ($test_name_part =~ /\@/) {
    # $^O on Windows considered not generic enough.
    my $plat = (IS_WINDOWS) ? 'windows' : $^O;

    # Test name part contains platform name.
    if ($test_name_part =~ /^\s*(\S+)\s+\@$plat\s*$/) {
      # Platform name is mentioned on which the test should be disabled
      # i.e "suite_name.test_name @platform : <comment>".
      $test_name = $1;
      validate_test_name($test_name, $line, $line_number, $disabled_def_file);
      return $test_name;
    } elsif ($test_name_part =~ /^\s*(\S+)\s+\@!(\S*)\s*$/) {
      # Platform name is mentioned on which the test shouldn't be
      # disabled i.e "suite_name.test_name @!platform : XXXX".
      $test_name = $1;
      validate_test_name($test_name, $line, $line_number, $disabled_def_file);
      return $test_name if ($2 ne $plat);
    } elsif ($test_name_part !~ /^\s*(\S+)\s+\@(\S*)\s*$/) {
      # Invalid format for a disabled test.
      report_disabled_test_format_error($line, $line_number,
                                        $disabled_def_file);
    }
  } elsif ($test_name_part =~ /^\s*(\S+)\s*$/) {
    # No platform specified.
    $test_name = $1;
    validate_test_name($test_name, $line, $line_number, $disabled_def_file);
    return $test_name;
  } else {
    # Invalid format, throw an error.
    report_disabled_test_format_error($line, $line_number, $disabled_def_file);
  }
}

## Check that a test specified in disabled.def actually exists
## if the suite it belongs to is collected.
##
## Arguments:
##   $test_name      Name of the test to be located
sub validate_test_existence($) {
  my $test_name = shift;
  my ($sname, $tname, $extension) = split_testname($test_name);

  # Search if the specified suite is present in the list
  # of suites which MTR collects
  for my $suite (split(",", $::opt_suites)) {
    if ($sname eq $suite or $sname eq "i_" . $suite) {
      # Proceed to check if the disabled test exists only if its
      # suite is picked up
      my $suitedir = get_suite_dir($sname);
      mtr_error("Disabled test '$test_name' could not be located.")
        if $suitedir and
        not my_find_file($suitedir, [ "", "t" ], "$tname.test", NOT_REQUIRED);
      last;
    }
  }
}

## Check if the comment section in a disabled.def file is correct. The
## format is "<BUG|WL>#<XXXX> [<comment>]". If the format is incorrect,
## throw an error and abort the test run.
##
## Arguments:
##   $comment_part      Comment section in a disabled test entry
##   $line              An entry from a disabled.def file
##   $line_number       Line number
##   $disabled_def_file disabled.def file location
sub validate_comment_part($$$$) {
  my $comment_part      = shift;
  my $line              = shift;
  my $line_number       = shift;
  my $disabled_def_file = shift;

  if ($comment_part =~ /^\s*(BUG|WL)#\d+\s*(\s+(.*))?$/i) {
    my $comment = $3;
    # Length of a comment section can't be more than 80 characters.
    if (length($comment) > 79) {
      mtr_error("Length of a comment section in a disabled test entry can't " .
                "be more than 80 characters, " .
                "'$line' at '$disabled_def_file:$line_number'.");
    }
    # Valid format for comment section.
    return $comment_part;
  } else {
    # Invalid format. throw an error.
    report_disabled_test_format_error($line, $line_number, $disabled_def_file);
  }
}

## Validate the format of an entry in a disabled.def file.
##
## Arguments:
##   $line              An entry from a disabled.def file
##   $line_number       Line number
##   $disabled_def_file disabled.def file location
##
## Returns:
##   Test name and the comment part.
sub validate_disabled_test_entry($$$) {
  my $line              = shift;
  my $line_number       = shift;
  my $disabled_def_file = shift;

  my $comment_part;
  my $test_name_part;

  if ($line =~ /^(.*?):(.*)$/) {
    $test_name_part = $1;
    $comment_part   = $2;
  }

  # Check the format of test name part.
  my $test_name =
    validate_test_name_part($test_name_part, $line, $line_number,
                            $disabled_def_file);

  # Check if the disabled test exists
  validate_test_existence($test_name) if $test_name;

  # Check the format of comment part.
  my $comment =
    validate_comment_part($comment_part, $line, $line_number,
                          $disabled_def_file);

  return ($test_name, $comment);
}

## Create a list of disabled tests from disabled.def file. This list
## is used while collecting the test cases. If a test case is disabled,
## disabled flag is enabled for the test and the test run will be
## disabled.
##
## Arguments:
##   $disabled           List to store the disabled tests
##   $opt_skip_test_list File containing list of tests to be disabled
##
## Returns:
##   List of disabled tests
sub create_disabled_test_list($$) {
  my $disabled           = shift;
  my $opt_skip_test_list = shift;

  if ($opt_skip_test_list) {
    $opt_skip_test_list = get_bld_path($opt_skip_test_list);
  }

  # Array containing files listing tests that should be disabled.
  my @disabled_collection = $opt_skip_test_list if $opt_skip_test_list;

  # Add 'disabled.def' files.
  unshift(@disabled_collection,
          "$::glob_mysql_test_dir/collections/disabled.def");

  # Add internal 'disabled.def' file only if it exists
  my $internal_disabled_def_file =
    "$::basedir/internal/mysql-test/collections/disabled.def";
  unshift(@disabled_collection, $internal_disabled_def_file)
    if -f $internal_disabled_def_file;

  # 'disabled.def' file in cloud directory.
  $internal_disabled_def_file =
    "$::basedir/internal/cloud/mysql-test/collections/disabled.def";
  unshift(@disabled_collection, $internal_disabled_def_file)
    if -f $internal_disabled_def_file;

  # Add 'disabled_ndb.def' to the list of disabled files if ndb is enabled.
  unshift(@disabled_collection,
          "$::glob_mysql_test_dir/collections/disabled_ndb.def")
    if $::ndbcluster_enabled;

  # Check for the tests to be skipped in a sanitizer which are listed
  # in "mysql-test/collections/disabled-<sanitizer>.list" file.
  if ($::opt_sanitize) {
    # Check for disabled-asan.list
    if ($::mysql_version_extra =~ /asan/i &&
        $opt_skip_test_list !~ /disabled-asan\.list$/) {
      push(@disabled_collection, "collections/disabled-asan.list");
    }
    # Check for disabled-ubsan.list
    elsif ($::mysql_version_extra =~ /ubsan/i &&
           $opt_skip_test_list !~ /disabled-ubsan\.list$/) {
      push(@disabled_collection, "collections/disabled-ubsan.list");
    }
  }

  # Check for the tests to be skipped in valgrind which are listed
  # in "mysql-test/collections/disabled-valgrind.list" file.
  if ($::opt_valgrind) {
    # Check for disabled-valgrind.list
    if ($opt_skip_test_list !~ /disabled-valgrind\.list$/) {
      push(@disabled_collection, "collections/disabled-valgrind.list");
    }
  }

  for my $disabled_def_file (@disabled_collection) {
    my $file_handle = IO::File->new($disabled_def_file, '<') or
      mtr_error("Can't open '$disabled_def_file' file: $!.");

    if (defined $file_handle) {
      while (my $line = <$file_handle>) {
        # Skip a line if it starts with '#' or is an empty line.
        next if ($line =~ /^#/ or $line =~ /^$/);

        chomp($line);
        my $line_number = $.;

        # Check the format of an entry in a disabled.def file.
        my ($test_name, $comment) =
          validate_disabled_test_entry($line, $line_number, $disabled_def_file);

        # Disable the test case if defined.
        $disabled->{$test_name} = $comment if defined $test_name;
      }
      $file_handle->close();
    }
  }
}

# This is the top level collection routine. If tests are explicitly
# named on the command line, it collects suite name information from
# the test names if all of them are qualified with suite name.
#
# Then it loops over all the suites to use (which is either the
# default or an explicit list), collecting tests using
# collect_one_suite.
#
# After collecting all the tests, it goes through all of them and
# constructs a sort criteria string for each, which is finally used
# to reorder the list in a way which is intended to reduce the number
# of restarts if 'reorder' option is enabled.
sub collect_test_cases ($$$$) {
  my $opt_reorder        = shift;    # True if we're reordering tests
  my $suites             = shift;    # Semicolon separated list of test suites
  my $opt_cases          = shift;
  my $opt_skip_test_list = shift;
  my $cases              = [];       # Array of hash(one hash for each testcase)

  # Unit tests off by default also if using --do-test or --start-from
  $::opt_ctest = 0 if $::opt_ctest == -1 && ($do_test || $start_from);

  $do_test_reg   = init_pattern($do_test,   "--do-test");
  $skip_test_reg = init_pattern($skip_test, "--skip-test");

  $lib_innodb_plugin =
    my_find_file($::basedir,
                 [ "storage/innodb_plugin", "storage/innodb_plugin/.libs",
                   "lib/mysql/plugin",      "lib/plugin"
                 ],
                 [ "ha_innodb_plugin.dll", "ha_innodb_plugin.so",
                   "ha_innodb_plugin.sl"
                 ],
                 NOT_REQUIRED);

  $do_innodb_plugin =
    ($::mysql_version_id >= 50100 && !(IS_WINDOWS) && $lib_innodb_plugin);

  # Build a hash of disabled testcases
  my %disabled;
  create_disabled_test_list(\%disabled, $opt_skip_test_list);

  # If not reordering, we also shouldn't group by suites, unless no
  # test cases were named. This also effects some logic in the loop
  # following this.
  if ($opt_reorder or !@$opt_cases) {
    my $parallel = $ENV{NUMBER_OF_CPUS};
    $parallel = $::opt_parallel if ($::opt_parallel < $parallel);
    $parallel = 1               if $quick_collect;
    $parallel = 1               if @$opt_cases;

    if ($parallel == 1 or !$threads_support or !$threads_shared_support) {
      foreach my $suite (split(",", $suites)) {
        push(@$cases,
             collect_one_suite($suite,              $opt_cases,
                               $opt_skip_test_list, \%disabled
             ));
        last if $some_test_found;
        push(@$cases,
             collect_one_suite("i_" . $suite,       $opt_cases,
                               $opt_skip_test_list, \%disabled
             ));
      }
    } else {
      share(\$group_replication);
      share(\$some_test_found);

      # Array containing thread id of all the threads used for
      # collecting test cases from different test suites.
      my @collect_test_cases_thrds;
      foreach my $suite (split(",", $suites)) {
        push(@collect_test_cases_thrds,
             threads->create("collect_one_suite", $suite,
                             $opt_cases,          $opt_skip_test_list,
                             \%disabled
             ));
        while ($parallel <= scalar @collect_test_cases_thrds) {
          mtr_milli_sleep(100);
          @collect_test_cases_thrds = threads->list(threads::running());
        }
        last if $some_test_found;

        push(@collect_test_cases_thrds,
             threads->create("collect_one_suite", "i_" . $suite,
                             $opt_cases,          $opt_skip_test_list,
                             \%disabled
             ));
        while ($parallel <= scalar @collect_test_cases_thrds) {
          mtr_milli_sleep(100);
          @collect_test_cases_thrds = threads->list(threads::running());
        }
      }

      foreach my $collect_thrd (threads->list()) {
        my @suite_cases = $collect_thrd->join();
        push(@$cases, @suite_cases);
      }
    }
  }

  if (@$opt_cases) {
    # A list of tests was specified on the command line. Among those,
    # the tests which are not already collected will be collected and
    # stored temporarily in an array of hashes pointed by the below
    # reference. This array is eventually appeneded to the one having
    # all collected test cases.
    my $cmdline_cases;

    # Check that the tests specified were found in at least one suite.
    foreach my $test_name_spec (@$opt_cases) {
      my $found = 0;
      my ($sname, $tname, $extension) = split_testname($test_name_spec);

      foreach my $test (@$cases) {
        last unless $opt_reorder;
        # 'test->{name}' value is always in suite.name format
        if ($test->{name} =~ /^$sname.*\.$tname$/) {
          $found = 1;
          last;
        }
      }

      if (not $found) {
        if ($sname) {
          # If suite was part of name, find it there, may come with combinations
          my @this_case = collect_one_suite($sname, [$tname]);

          # If a test is specified multiple times on the command line, all
          # instances of the test need to be picked. Hence, such tests are
          # stored in the temporary array instead of adding them to $cases
          # directly so that repeated tests are not run only once
          if (@this_case) {
            push(@$cmdline_cases, @this_case);
          } else {
            mtr_error("Could not find '$tname' in '$sname' suite");
          }
        } else {
          if (!$opt_reorder) {
            # If --no-reorder is passed and if suite was not part of name,
            # search in all the suites
            foreach my $suite (split(",", $suites)) {
              my @this_case = collect_one_suite($suite, [$tname]);
              if (@this_case) {
                push(@$cmdline_cases, @this_case);
                $found = 1;
              }
              @this_case = collect_one_suite("i_" . $suite, [$tname]);
              if (@this_case) {
                push(@$cmdline_cases, @this_case);
                $found = 1;
              }
            }
          }
          if (!$found) {
            mtr_error("Could not find '$tname' in '$suites' suite(s)");
          }
        }
      }
    }

    # Add test cases collected in the temporary array to the one
    # containing all previously collected test cases
    push(@$cases, @$cmdline_cases) if $cmdline_cases;
  }

  if ($opt_reorder && !$quick_collect) {
    # Reorder the test cases in an order that will make them faster to
    # run. Make a mapping of test name to a string that represents how
    # that test should be sorted among the other tests. Put the most
    # important criterion first, then a sub-criterion, then
    # sub-sub-criterion, etc.
    foreach my $tinfo (@$cases) {
      my @criteria = ();

      # Append the criteria for sorting, in order of importance.
      push(@criteria, "ndb=" . ($tinfo->{'ndb_test'} ? "A" : "B"));

      push(@criteria, $tinfo->{template_path});

      # Group test with equal options together. Ending with "~" makes
      # empty sort later than filled.
      my $opts = $tinfo->{'master_opt'} ? $tinfo->{'master_opt'} : [];
      push(@criteria, join("!", sort @{$opts}) . "~");
      # Add slave opts if any
      if ($tinfo->{'slave_opt'}) {
        push(@criteria, join("!", sort @{ $tinfo->{'slave_opt'} }));
      }

      # This sorts tests with force-restart *before* identical tests
      push(@criteria, $tinfo->{force_restart} ? "force-restart" : "no-restart");

      $tinfo->{criteria} = join(" ", @criteria);
    }

    @$cases = sort { $a->{criteria} cmp $b->{criteria}; } @$cases;
  }

  if ($::opt_repeat > 1) {
    $cases = duplicate_test_cases($cases);
  }

  if (defined $print_testcases) {
    print_testcases(@$cases);
    exit(1);
  }

  return $cases;
}

# Duplicate each test $opt_repeat number of times
sub duplicate_test_cases($) {
  my $tests = shift;

  my $new_tests = [];
  foreach my $test (@$tests) {
    # Don't repeat the test if 'skip' flag is enabled.
    if ($test->{'skip'}) {
      push(@{$new_tests}, $test);
    } else {
      for (my $i = 1 ; $i <= $::opt_repeat ; $i++) {
        # Create a duplicate test object
        push(@{$new_tests}, create_duplicate_test($test));
      }
    }
  }

  return $new_tests;
}

# Create a new test object identical to the original one.
sub create_duplicate_test($) {
  my $test = shift;

  my $new_test = My::Test->new();
  while (my ($key, $value) = each(%$test)) {
    if (ref $value eq "ARRAY") {
      push(@{ $new_test->{$key} }, @$value);
    } else {
      $new_test->{$key} = $value;
    }
  }

  return $new_test;
}

# Returns (suitename, testname, extension)
sub split_testname {
  my ($test_name) = @_;

  # If .test file name is used, get rid of directory part
  $test_name = basename($test_name) if $test_name =~ /\.test$/;

  # Now split name on .'s
  my @parts = split(/\./, $test_name);

  if (@parts == 1) {
    # Only testname given, ex: alias
    return (undef, $parts[0], undef);
  } elsif (@parts == 2) {
    # Either testname.test or suite.testname given.
    # E.g. main.alias or alias.test
    if ($parts[1] eq "test") {
      return (undef, $parts[0], $parts[1]);
    } else {
      return ($parts[0], $parts[1], undef);
    }

  } elsif (@parts == 3) {
    # Fully specified suitename.testname.test, E.g. main.alias.test
    return ($parts[0], $parts[1], $parts[2]);
  }

  mtr_error("Illegal format of test name: $test_name");
}

# Read a combination file and return an array of all possible
# combinations.
sub combinations_from_file($) {
  my $combination_file = shift;

  my @combinations;

  # Read combinations file in my.cnf format
  mtr_verbose("Read combinations file $combination_file.");

  my $config = My::Config->new($combination_file);
  foreach my $group ($config->groups()) {
    my $comb = {};
    $comb->{name} = $group->name();
    foreach my $option ($group->options()) {
      push(@{ $comb->{comb_opt} }, $option->option());
    }
    push(@combinations, $comb);
  }

  return @combinations;
}

# Fetch different combinations specified on command line using
# --combination option.
sub combinations_from_command_line($) {
  my @opt_combinations = shift;

  # Take the combination from command-line
  mtr_verbose("Take the combination from command line");

  # Collect all combinations
  my @combinations;
  foreach my $combination (@::opt_combinations) {
    my $comb = {};
    $comb->{name} = $combination;
    push(@{ $comb->{comb_opt} }, $combination);
    push(@combinations,          $comb);
  }

  return @combinations;
}

# Create a new test object for each combination and return an array
# containing all new tests.
sub create_test_combinations($$) {
  my $test         = shift;
  my $combinations = shift;

  my @new_cases;

  foreach my $comb (@{$combinations}) {
    # Skip this combination if the values it provides already are set
    # in master_opt or slave_opt.
    if (My::Options::is_set($test->{master_opt}, $comb->{comb_opt}) ||
        My::Options::is_set($test->{slave_opt},          $comb->{comb_opt}) ||
        My::Options::is_set(\@::opt_extra_bootstrap_opt, $comb->{comb_opt}) ||
        My::Options::is_set(\@::opt_extra_mysqld_opt,    $comb->{comb_opt})) {
      next;
    }

    # Create a new test object.
    my $new_test = My::Test->new();

    # Copy the original test options.
    while (my ($key, $value) = each(%$test)) {
      if (ref $value eq "ARRAY") {
        push(@{ $new_test->{$key} }, @$value);
      } else {
        $new_test->{$key} = $value;
      }
    }

    # Append the combination options to master_opt and slave_opt
    push(@{ $new_test->{master_opt} }, @{ $comb->{comb_opt} })
      if defined $comb->{comb_opt};
    push(@{ $new_test->{slave_opt} }, @{ $comb->{comb_opt} })
      if defined $comb->{comb_opt};

    # Add combination name
    $new_test->{combination} = $comb->{name};

    # Add the new test to list of new test cases
    push(@new_cases, $new_test);
  }

  return @new_cases;
}

# Look through one test suite for tests named in the second parameter,
# or all tests in the suite if that list is empty.
#
# It starts by finding the proper directory, then handle any
# disabled.def file found. Then loop over either the list of test names
# or all files in the suite directory ending with ".test", adding them
# using collect_one_test_case().
#
# After the list is complete (in local variable @cases), check if
# combinations are being used, from command line or from a combination
# file. If so, start building a new list of test cases in @new_cases
# using those combinations, then assigns that over to @cases.
sub collect_one_suite($$$$) {
  my $suite              = shift;    # Test suite name
  my $opt_cases          = shift;
  my $opt_skip_test_list = shift;
  my $disabled           = shift;
  my @cases;                         # Array of hash

  mtr_verbose("Collecting: $suite");

  # Default suite(i.e main suite) directory location
  my $suitedir = "$::glob_mysql_test_dir";

  if ($suite ne "main") {
    # Allow suite to be path to "some dir" if $suite has at least
    # one directory part
    if (-d $suite and splitdir($suite) > 1) {
      $suitedir = $suite;
      mtr_report(" - from '$suitedir'");

    } else {
      $suitedir = get_suite_dir($suite);
      return unless $suitedir;
    }
    mtr_verbose("suitedir: $suitedir");
  }

  my $resdir  = "$suitedir/r";
  my $testdir = "$suitedir/t";

  # Check if test directory ('t/') exists
  if (-d $testdir) {
    # Test directory exists
    if (-d $resdir) {
      # Result directory ('r/') exists
    } else {
      # Result directory doesn't exist, use test dorectory as
      # result directory
      $resdir = $testdir;
    }
  } else {
    # Test directory doesn't exists, so there can't be a resullt
    # directory as well.
    mtr_error("Can't have r/ dir without t/") if -d $resdir;

    # No t/ or r/ => use suitedir
    $resdir = $testdir = $suitedir;
  }

  mtr_verbose("testdir: $testdir");
  mtr_verbose("resdir: $resdir");

  # Read suite.opt file
  my $suite_opt_file = "$testdir/suite.opt";

  if ($::opt_suite_opt) {
    $suite_opt_file = "$testdir/$::opt_suite_opt";
  }

  my $suite_opts = [];
  if (-f $suite_opt_file) {
    $suite_opts = opts_from_file($suite_opt_file);
  }

  if (@$opt_cases) {
    # Collect in specified order
    foreach my $test_name_spec (@$opt_cases) {
      my ($sname, $tname, $extension) = split_testname($test_name_spec);

      # Check correct suite if suitename is defined
      next if (defined $sname and $suite ne $sname);

      if (defined $extension) {
        my $full_name = "$testdir/$tname.$extension";
        # Extension was specified, check if the test exists
        if (!-f $full_name) {
          # This is only an error if suite was specified, otherwise it
          # could exist in another suite
          mtr_error("Test '$full_name' was not found in suite '$sname'")
            if $sname;
          next;
        }
      } else {
        # No extension was specified, use default
        $extension = "test";
        my $full_name = "$testdir/$tname.$extension";

        # Test not found here, could exist in other suite
        next if (!-f $full_name);
      }

      push(@cases,
           collect_one_test_case($suitedir, $testdir,
                                 $resdir,   $suite,
                                 $tname,    "$tname.$extension",
                                 $disabled, $suite_opts
           ));
    }
  } else {
    opendir(TESTDIR, $testdir) or mtr_error("Can't open dir \"$testdir\": $!");

    foreach my $elem (sort readdir(TESTDIR)) {
      my $tname = mtr_match_extension($elem, 'test');
      next unless defined $tname;

      # Skip tests that does not match the --do-test= filter
      next if ($do_test_reg and not $tname =~ /$do_test_reg/o);

      push(@cases,
           collect_one_test_case($suitedir, $testdir, $resdir,
                                 $suite,    $tname,   $elem,
                                 $disabled, $suite_opts
           ));
    }
    closedir TESTDIR;
  }

  #  Return empty list if no testcases found
  return if (@cases == 0);

  # Read combinations for this suite and build testcases x
  # combinations if any combinations exists.
  if (!$::opt_skip_combinations && !$quick_collect) {
    my @combinations;
    if (@::opt_combinations) {
      @combinations = combinations_from_command_line(@::opt_combinations);
    } else {
      my $combination_file = "$suitedir/combinations";
      @combinations = combinations_from_file($combination_file)
        if -f $combination_file;
    }

    if (@combinations) {
      my @new_cases;
      mtr_report(" - Adding combinations for $suite");

      foreach my $test (@cases) {
        next if ($test->{'skip'} or defined $test->{'combination'});
        push(@new_cases, create_test_combinations($test, \@combinations));
      }

      # Add the plain test if it was not already added as part
      # of a combination.
      my %added;
      foreach my $new_test (@new_cases) {
        $added{ $new_test->{name} } = 1;
      }

      foreach my $test (@cases) {
        push(@new_cases, $test) unless $added{ $test->{name} };
      }

      @cases = @new_cases;
    }
  }

  optimize_cases(\@cases);
  return @cases;
}

# Find location of the specified suite
sub get_suite_dir($) {
  my $suite = shift;
  return $::glob_mysql_test_dir if ($suite eq "main");
  return my_find_dir(
    $::basedir,
    [ "internal/cloud/mysql-test/suite/",
      "internal/mysql-test/suite/",
      "internal/plugin/$suite/tests",
      "lib/mysql-test/suite",
      "mysql-test/suite",
      # Look in plugin specific suite dir
      "plugin/$suite/tests",
      "share/mysql-test/suite",
    ],
    [ $suite, "mtr" ],
    # Allow reference to non-existing suite in PB2
    ($suite =~ /^i_/ || defined $ENV{PB2WORKDIR}));
}

# Set the skip flag in test object and add the skip comments.
sub skip_test {
  my $tinfo   = shift;
  my $comment = shift;

  $tinfo->{'skip'}    = 1;
  $tinfo->{'comment'} = $comment;
}

# Loop through the list of test cases and marks those that should be
# skipped if incompatible with what the server supports etc or update
# the test settings if necessary.
sub optimize_cases {
  my ($cases) = @_;

  foreach my $tinfo (@$cases) {
    # Skip processing if already marked as skipped
    next if $tinfo->{skip};

    # If a binlog format was set with '--mysqld=--binlog-format=x',
    # skip all tests that doesn't support it.
    if (defined $binlog_format) {
      # Fixed '--binlog-format=x' specified on command line
      if (defined $tinfo->{'binlog_formats'}) {
        # The test supports different binlog formats check if the
        # selected one is ok.
        my $supported =
          grep { $_ eq lc $binlog_format } @{ $tinfo->{'binlog_formats'} };
        if (!$supported) {
          $tinfo->{'skip'} = 1;
          $tinfo->{'comment'} =
            "Doesn't support --binlog-format='$binlog_format'";
        }
      }
    } else {
      # =======================================================
      # Use dynamic switching of binlog format
      # =======================================================

      # Get binlog-format used by this test from master_opt
      my $test_binlog_format;
      foreach my $opt (@{ $tinfo->{master_opt} }) {
        (my $dash_opt = $opt) =~ s/_/-/g;
        $test_binlog_format =
          mtr_match_prefix($dash_opt, "--binlog-format=") ||
          $test_binlog_format;
      }

      if (defined $test_binlog_format and
          defined $tinfo->{binlog_formats}) {
        my $supported =
          grep { My::Options::option_equals($_, lc $test_binlog_format) }
          @{ $tinfo->{'binlog_formats'} };
        if (!$supported) {
          skip_test($tinfo,
                    "Doesn't support --binlog-format = '$test_binlog_format'");
          next;
        }
      }
    }

    # Check that engine set by '--default-storage-engine=<engine>' is supported
    my %builtin_engines = ('myisam' => 1, 'memory' => 1, 'csv' => 1);

    foreach my $opt (@{ $tinfo->{master_opt} }) {
      (my $dash_opt = $opt) =~ s/_/-/g;

      my $default_engine =
        mtr_match_prefix($dash_opt, "--default-storage-engine=");
      my $default_tmp_engine =
        mtr_match_prefix($dash_opt, "--default-tmp-storage-engine=");

      # Allow use of uppercase, convert to all lower case
      $default_engine =~ tr/A-Z/a-z/;
      $default_tmp_engine =~ tr/A-Z/a-z/;

      if (defined $default_engine) {
        if (!exists $::mysqld_variables{$default_engine} and
            !exists $builtin_engines{$default_engine}) {
          skip_test($tinfo, "'$default_engine' not supported");
        }

        $tinfo->{'ndb_test'} = 1
          if ($default_engine =~ /^ndb/i);
        $tinfo->{'myisam_test'} = 1
          if ($default_engine =~ /^myisam/i);
      }

      if (defined $default_tmp_engine) {
        if (!exists $::mysqld_variables{$default_tmp_engine} and
            !exists $builtin_engines{$default_tmp_engine}) {
          skip_test($tinfo, "'$default_tmp_engine' not supported");
        }

        $tinfo->{'ndb_test'} = 1
          if ($default_tmp_engine =~ /^ndb/i);
        $tinfo->{'myisam_test'} = 1
          if ($default_tmp_engine =~ /^myisam/i);
      }

      if($secondary_engine_support) {
        optimize_secondary_engine_tests($dash_opt, $tinfo);
      }
    }

    if ($quick_collect && !$tinfo->{'skip'}) {
      $some_test_found = 1;
      return;
    }
  }
}

# Read options from a test options file and append them as an array
# to $tinfo->{$opt_name} member. Some options however have a different
# semantics for MTR instead of just being passed to the server.
# These are being handled here as well.
sub process_opts_file {
  my ($tinfo, $opt_file, $opt_name) = @_;

  if (-f $opt_file) {
    my $opts = opts_from_file($opt_file);

    foreach my $opt (@$opts) {
      my $value;

      # The opt file is used both to send special options to the
      # mysqld as well as pass special test case specific options to
      # this script.
      $value = mtr_match_prefix($opt, "--timezone=");
      if (defined $value) {
        $tinfo->{'timezone'} = $value;
        next;
      }

      $value = mtr_match_prefix($opt, "--result-file=");
      if (defined $value) {
        # Specifies the file mysqltest should compare output against.
        $tinfo->{'result_file'} = "r/$value.result";
        next;
      }

      $value = mtr_match_prefix($opt, "--config-file-template=");
      if (defined $value) {
        # Specifies the configuration file to use for this test
        $tinfo->{'template_path'} = dirname($tinfo->{path}) . "/$value";
        next;
      }

      # If we set default time zone, remove the one we have
      $value = mtr_match_prefix($opt, "--default-time-zone=");
      if (defined $value) {
        # Set timezone for this test case to something different
        $tinfo->{'timezone'} = "GMT-8";
        # Fallthrough, add the --default-time-zone option
      }

      # The --restart option forces a restart even if no special option
      # is set. If the options are the same as next testcase there is no
      # need to restart after the testcase has completed.
      if ($opt eq "--force-restart") {
        $tinfo->{'force_restart'} = 1;
        next;
      }

      $value = mtr_match_prefix($opt, "--testcase-timeout=");
      if (defined $value) {
        # Overrides test case timeout for this test
        $tinfo->{'case-timeout'} = $value;
        next;
      }

      if ($opt eq "--nowarnings") {
        $tinfo->{'skip_check_warnings'} = 1;
        next;
      }

      # Ok, this was a real option, add it
      push(@{ $tinfo->{$opt_name} }, $opt);
    }
  }
}

# Collect all necessary information about a single test and returns
# a new My::Test instance to be added to the global list.
#
# Most of the code is about finding and setting various test specific
# options or properties. Subroutine tags_from_test_file() uses a list
# of fixed patterns called "tags" to set the properties for a test
# case.
sub collect_one_test_case {
  my $suitedir   = shift;
  my $testdir    = shift;
  my $resdir     = shift;
  my $suitename  = shift;
  my $tname      = shift;
  my $filename   = shift;
  my $disabled   = shift;
  my $suite_opts = shift;

  # Test file name should consist of only alpha-numeric characters, dash (-)
  # or underscore (_), but should not start with dash or underscore.
  if ($tname !~ /^[^_\W][\w-]*$/) {
    die("Invalid test file name '$suitename.$tname'. Test file " .
        "name should consist of only alpha-numeric characters, " .
        "dash (-) or underscore (_), but should not start with " .
        "dash or underscore.");
  }

  # Check --start-from
  if ($start_from) {
    # start_from can be specified as [suite.].testname_prefix
    my ($suite, $test, $ext) = split_testname($start_from);

    # Skip silently
    return if (($suite and $suitename lt $suite) || ($tname lt $test));
  }

  # Set defaults
  my $tinfo = My::Test->new(name      => "$suitename.$tname",
                            path      => "$testdir/$filename",
                            shortname => $tname,);

  my $result_file = "$resdir/$tname.result";
  if (-f $result_file) {
    $tinfo->{result_file} = $result_file;
  } else {
    # Result file doesn't exist
    if ($::opt_check_testcases and !$::opt_record) {
      # Set 'no_result_file' flag if check-testcases is enabled.
      $tinfo->{'no_result_file'} = $result_file;
    } else {
      # No .result file exist, remember the path where it should
      # be saved in case of --record.
      $tinfo->{record_file} = $result_file;
    }
  }

  # Disable quiet output when a test file doesn't contain a result file
  # and --record option is disabled.
  if ($::opt_quiet and defined $tinfo->{record_file} and !$::opt_record) {
    $::opt_quiet = 0;
    mtr_report("Turning off '--quiet' option since the MTR run contains " .
               "a test without a result file.");
  }

  # Skip some tests but include in list, just mark them as skipped
  if ($skip_test_reg and $tname =~ /$skip_test_reg/o) {
    $tinfo->{'skip'} = 1;
    return $tinfo;
  }

  # Check for replicaton tests
  $tinfo->{'rpl_test'} = 1 if ($suitename =~ 'rpl');

  # Check for replication tests which need GTID mode OFF
  $tinfo->{'rpl_nogtid_test'} = 1 if ($suitename =~ 'rpl_nogtid');

  # Check for replication tests which need GTID mode ON
  $tinfo->{'rpl_gtid_test'} = 1 if ($suitename =~ 'rpl_gtid');

  # Check for group replication tests
  $tinfo->{'grp_rpl_test'} = 1 if ($suitename =~ 'group_replication');

  # Check for disabled tests
  if ($disabled->{"$suitename.$tname"}) {
    # Test was marked as disabled in disabled.def file
    $tinfo->{'comment'} = $disabled->{"$suitename.$tname"};
    if ($enable_disabled or @::opt_cases) {
      # User has selected to run all disabled tests
      mtr_report(" - Running test $tinfo->{name} even though it's been",
                 "disabled due to '$tinfo->{comment}'.");
    } else {
      $tinfo->{'skip'} = 1;
      # Disable the test case
      $tinfo->{'disable'} = 1;
      return $tinfo;
    }
  }

  # Append suite extra options to both master and slave
  push(@{ $tinfo->{'master_opt'} }, @$suite_opts);
  push(@{ $tinfo->{'slave_opt'} },  @$suite_opts);

  # Check for test specific config file
  my $test_cnf_file = "$testdir/$tname.cnf";
  if (-f $test_cnf_file) {
    # Specifies the configuration file to use for this test
    $tinfo->{'template_path'} = $test_cnf_file;
  }

  # master sh
  my $master_sh = "$testdir/$tname-master.sh";
  if (-f $master_sh) {
    if (IS_WIN32PERL) {
      $tinfo->{'skip'}    = 1;
      $tinfo->{'comment'} = "No tests with sh scripts on Windows";
      return $tinfo;
    } else {
      $tinfo->{'master_sh'} = $master_sh;
    }
  }

  # slave sh
  my $slave_sh = "$testdir/$tname-slave.sh";
  if (-f $slave_sh) {
    if (IS_WIN32PERL) {
      $tinfo->{'skip'}    = 1;
      $tinfo->{'comment'} = "No tests with sh scripts on Windows";
      return $tinfo;
    } else {
      $tinfo->{'slave_sh'} = $slave_sh;
    }
  }

  # <tname>.slave-mi
  mtr_error("$tname: slave-mi not supported anymore")
    if (-f "$testdir/$tname.slave-mi");

  tags_from_test_file($tinfo, "$testdir/${tname}.test");

  # Check that test wth "ndb" in their suite name
  # have been tagged as 'ndb_test', this is normally fixed
  # by adding a "source include/have_ndb.inc" to the file
  if ($suitename =~ /ndb/) {
    mtr_error("The test '$tinfo->{name}' is not tagged as 'ndb_test'")
      unless ($tinfo->{'ndb_test'});
  }

  # Disable the result file check for NDB tests not having its
  # corresponding result file.
  if ($tinfo->{'ndb_test'} and $tinfo->{'ndb_no_result_file_test'}) {
    delete $tinfo->{'no_result_file'} if $tinfo->{'no_result_file'};
  }

  if (defined $default_storage_engine) {
    # Different default engine is used tag test to require that engine.
    $tinfo->{'ndb_test'} = 1
      if ($default_storage_engine =~ /^ndb/i);
    $tinfo->{'mysiam_test'} = 1
      if ($default_storage_engine =~ /^mysiam/i);
  }

  # Except the tests which need big-test or only-big-test option to run
  # in valgrind environment(i.e tests having no_valgrind_without_big.inc
  # include file), other normal/non-big tests shouldn't run with
  # only-big-test option.
  if ($::opt_only_big_test) {
    if ((!$tinfo->{'no_valgrind_without_big'} and !$tinfo->{'big_test'}) or
        ($tinfo->{'no_valgrind_without_big'} and !$::opt_valgrind)) {
      skip_test($tinfo, "Not a big test");
      return $tinfo;
    }
  }

  # Check for big test
  if ($tinfo->{'big_test'} and !($::opt_big_test or $::opt_only_big_test)) {
    skip_test($tinfo, "Test needs 'big-test' or 'only-big-test' option.");
    return $tinfo;
  }

  # Tests having no_valgrind_without_big.inc include file needs either
  # big-test or only-big-test option to run in valgrind environment.
  if ($tinfo->{'no_valgrind_without_big'} and $::opt_valgrind) {
    if (!$::opt_big_test and !$::opt_only_big_test) {
      skip_test($tinfo,
                "Need '--big-test' or '--only-big-test' when running " .
                  "with Valgrind.");
      return $tinfo;
    }
  }

  if ($tinfo->{'need_debug'} && !$::debug_compiled_binaries) {
    skip_test($tinfo, "Test needs debug binaries.");
    return $tinfo;
  }

  if ($tinfo->{'asan_need_debug'} && !$::debug_compiled_binaries) {
    if ($::mysql_version_extra =~ /asan/) {
      skip_test($tinfo, "Test needs debug binaries if built with ASAN.");
      return $tinfo;
    }
  }

  if ($tinfo->{'need_backup'}) {
    if (!$::mysqlbackup_enabled) {
      skip_test($tinfo, "Test needs mysqlbackup.");
      return $tinfo;
    }
  }

  if ($tinfo->{'ndb_test'}) {
    # This is a NDB test
    if ($::ndbcluster_enabled == 0) {
      # ndbcluster is disabled
      skip_test($tinfo, "ndbcluster disabled");
      return $tinfo;
    }
  } else {
    # This is not a ndb test
    if ($::ndbcluster_only) {
      # Only the ndb test should be run, all other should be skipped
      skip_test($tinfo, "Only ndbcluster tests");
      return $tinfo;
    }
  }

  if ($tinfo->{'federated_test'}) {
    # This is a test that needs federated, enable it
    push(@{ $tinfo->{'master_opt'} }, "--loose-federated");
    push(@{ $tinfo->{'slave_opt'} },  "--loose-federated");
  }

  if ($tinfo->{'myisam_test'}) {
    # This is a temporary fix to allow non-innodb tests to run even if
    # the default storage engine is innodb.
    push(@{ $tinfo->{'master_opt'} }, "--default-storage-engine=MyISAM");
    push(@{ $tinfo->{'slave_opt'} },  "--default-storage-engine=MyISAM");
    push(@{ $tinfo->{'master_opt'} }, "--default-tmp-storage-engine=MyISAM");
    push(@{ $tinfo->{'slave_opt'} },  "--default-tmp-storage-engine=MyISAM");
  }

  if ($tinfo->{'need_binlog'}) {
    if (grep(/^--skip[-_]log[-_]bin/, @::opt_extra_mysqld_opt)) {
      skip_test($tinfo, "Test needs binlog");
      return $tinfo;
    }
  }

  if ($tinfo->{'rpl_test'} or $tinfo->{'grp_rpl_test'}) {
    if ($skip_rpl) {
      skip_test($tinfo, "No replication tests, --skip-rpl is enabled.");
      return $tinfo;
    }
  }

  # Check for group replication tests
  $group_replication = 1 if ($tinfo->{'grp_rpl_test'});

  if ($tinfo->{'not_windows'} && IS_WINDOWS) {
    skip_test($tinfo, "Test not supported on Windows");
    return $tinfo;
  }

  # Find config file to use if not already selected in <testname>.opt file
  if (defined $defaults_file) {
    # Using same config file for all tests
    $defaults_file = get_bld_path($defaults_file);
    $tinfo->{template_path} = $defaults_file;
  } elsif (!$tinfo->{template_path}) {
    my $config = "$suitedir/my.cnf";
    if (!-f $config) {
      # Assume default.cnf will be used
      $config = "include/default_my.cnf";

      # rpl_gtid tests must use their suite's cnf file having gtid mode on.
      if ($tinfo->{rpl_gtid_test}) {
        $config = "suite/rpl_gtid/my.cnf";
      }
      # rpl_nogtid tests must use their suite's cnf file having gtid mode off.
      elsif ($tinfo->{rpl_nogtid_test}) {
        $config = "suite/rpl_nogtid/my.cnf";
      }
      # rpl tests must use their suite's cnf file.
      elsif ($tinfo->{rpl_test}) {
        $config = "suite/rpl/my.cnf";
      }
    }
    $tinfo->{template_path} = $config;
  }

  # Set extra config file to use
  if (defined $defaults_extra_file) {
    $defaults_extra_file = get_bld_path($defaults_extra_file);
    $tinfo->{extra_template_path} = $defaults_extra_file;
  }

  if (!$::start_only or @::opt_cases) {
    # Add master opts, extra options only for master
    process_opts_file($tinfo, "$testdir/$tname-master.opt", 'master_opt');

    # Add slave opts, list of extra option only for slave
    process_opts_file($tinfo, "$testdir/$tname-slave.opt", 'slave_opt');
  }

  if (!$::start_only) {
    # Add client opts, extra options only for mysqltest client
    process_opts_file($tinfo, "$testdir/$tname-client.opt", 'client_opt');
  }

  my @new_tests;

  # Check if test specific combination file (<test_name>.combinations)
  # exists, if yes read different combinations from it.
  if (!$::opt_skip_combinations && !$quick_collect) {
    if (!$tinfo->{'skip'}) {
      my $combination_file = "$testdir/$tname.combinations";

      # Check for test specific combination file
      my @combinations = combinations_from_file($combination_file)
        if -f $combination_file;

      # Create a new test object for each combination
      if (@combinations) {
        mtr_report(" - Adding combinations for test '$tname'");
        push(@new_tests, create_test_combinations($tinfo, \@combinations));
      }
    }
  }

  @new_tests ? return @new_tests : return $tinfo;
}

# List of tags in the .test files that if found should set
# the specified value in "tinfo"
my @tags = (
  [ "include/have_binlog_format_row.inc", "binlog_formats", ["row"] ],
  [ "include/have_binlog_format_statement.inc", "binlog_formats",
    ["statement"]
  ],
  [ "include/have_binlog_format_mixed.inc", "binlog_formats",
    [ "mixed", "mix" ]
  ],
  [ "include/have_binlog_format_mixed_or_row.inc", "binlog_formats",
    [ "mixed", "mix", "row" ]
  ],
  [ "include/have_binlog_format_mixed_or_statement.inc", "binlog_formats",
    [ "mixed", "mix", "statement" ]
  ],
  [ "include/have_binlog_format_row_or_statement.inc", "binlog_formats",
    [ "row", "statement" ]
  ],

  [ "include/have_log_bin.inc", "need_binlog", 1 ],

  # An empty file to use test that needs myisam engine.
  [ "include/force_myisam_default.inc", "myisam_test", 1 ],

  [ "include/big_test.inc",       "big_test",   1 ],
  [ "include/asan_have_debug.inc","asan_need_debug", 1 ],
  [ "include/have_backup.inc",    "need_backup", 1 ],
  [ "include/have_debug.inc",     "need_debug", 1 ],
  [ "include/have_ndb.inc",       "ndb_test",   1 ],
  [ "include/have_multi_ndb.inc", "ndb_test",   1 ],

  # Any test sourcing the below inc file is considered to be an NDB
  # test not having its corresponding result file.
  [ "include/ndb_no_result_file.inc", "ndb_no_result_file_test", 1 ],

  # The tests with below four .inc files are considered to be rpl tests.
  [ "include/rpl_init.inc",    "rpl_test", 1 ],
  [ "include/rpl_ip_mix.inc",  "rpl_test", 1 ],
  [ "include/rpl_ip_mix2.inc", "rpl_test", 1 ],
  [ "include/rpl_ipv6.inc",    "rpl_test", 1 ],

  [ "include/ndb_master-slave.inc", "ndb_test",       1 ],
  [ "federated.inc",                "federated_test", 1 ],
  [ "include/not_windows.inc",      "not_windows",    1 ],
  [ "include/not_parallel.inc",     "not_parallel",   1 ],

  # Tests with below .inc file are considered to be group replication tests
  [ "have_group_replication_plugin_base.inc", "grp_rpl_test", 1 ],
  [ "have_group_replication_plugin.inc",      "grp_rpl_test", 1 ],

  # Tests with below .inc file needs either big-test or only-big-test
  # option along with valgrind option.
  [ "include/no_valgrind_without_big.inc", "no_valgrind_without_big", 1 ]);

if ($secondary_engine_support) {
  push (@tags, get_secondary_engine_tags());
}

sub tags_from_test_file {
  my $tinfo = shift;
  my $file  = shift;

  my $F = IO::File->new($file) or mtr_error("can't open file \"$file\": $!");
  while (my $line = <$F>) {
    # Skip line if it start's with '#'
    next if ($line =~ /^#/);

    # Match this line against tag in "tags" array
    foreach my $tag (@tags) {
      if (index($line, $tag->[0]) >= 0) {
        # Tag matched, assign value to "tinfo"
        $tinfo->{"$tag->[1]"} = $tag->[2];
      }
    }

    # If test sources another file, open it as well
    if ($line =~ /^\-\-([[:space:]]*)source(.*)$/ or
        $line =~ /^([[:space:]]*)source(.*);$/) {
      my $value = $2;
      $value =~ s/^\s+//;             # Remove leading space
      $value =~ s/[[:space:]]+$//;    # Remove ending space

      # Sourced file may exist relative to test or in global location
      foreach my $sourced_file (dirname($file) . "/$value",
                                "$::glob_mysql_test_dir/$value") {
        if (-f $sourced_file) {
          # Only source the file if it exists, we may get false
          # positives in the regexes above if someone writes
          # "source nnnn;" in a test case(such as mysqltest.test).
          tags_from_test_file($tinfo, $sourced_file);
          last;
        }
      }
    }
  }
}

sub unspace {
  my $string = shift;
  my $quote  = shift;
  $string =~ s/[ \t]/\x11/g;
  return "$quote$string$quote";
}

sub opts_from_file ($) {
  my $file = shift;

  my $file_handle = IO::File->new($file, '<') or
    mtr_error("Can't open file '$file': $!");

  my @args;
  while (<$file_handle>) {
    # Skip a line if it starts with '#' (i.e comments)
    next if /^\s*#/;

    chomp;
    s/^\s+//;    # Remove leading space
    s/\s+$//;    # Remove ending space

    # This is strange, but we need to fill whitespace inside quotes
    # with something, to remove later. We do this to be able to split
    # on space. Else, we have trouble with options like
    # --someopt="--insideopt1 --insideopt2". But still with this, we
    # are not 100% sure it is right, we need a shell to do it right.
    s/\'([^\'\"]*)\'/unspace($1,"\x0a")/ge;
    s/\"([^\'\"]*)\"/unspace($1,"\x0b")/ge;
    s/\'([^\'\"]*)\'/unspace($1,"\x0a")/ge;
    s/\"([^\'\"]*)\"/unspace($1,"\x0b")/ge;

    foreach my $arg (split(/[ \t]+/)) {
      $arg =~ tr/\x11\x0a\x0b/ \'\"/;

      # Put back real chars, the outermost quotes has to go
      $arg =~ s/^([^\'\"]*)\'(.*)\'([^\'\"]*)$/$1$2$3/ or
        $arg =~ s/^([^\'\"]*)\"(.*)\"([^\'\"]*)$/$1$2$3/;
      $arg =~ s/\\\\/\\/g;

      # Do not pass empty string since my_getopt is not capable to
      # handle it.
      push(@args, $arg) if (length($arg));
    }
  }

  $file_handle->close();
  return \@args;
}

sub print_testcases {
  my (@cases) = @_;

  print "=" x 60, "\n";
  foreach my $test (@cases) {
    $test->print_test();
  }
  print "=" x 60, "\n";
}

1;
