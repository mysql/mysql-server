# -*- cperl -*-
# Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.
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

my $threads_support= eval 'use threads; 1';
my $threads_shared_support= eval 'use threads::shared; 1';

use base qw(Exporter);
our @EXPORT= qw(collect_option collect_test_cases init_pattern
                $group_replication $xplugin);

use mtr_report;
use mtr_match;

# Options used for the collect phase
our $start_from;
our $print_testcases;
our $skip_rpl;
our $do_test;
our $skip_test;
our $skip_combinations;
our $binlog_format;
our $enable_disabled;
our $default_storage_engine;
our $opt_with_ndbcluster_only;
our $defaults_file;
our $defaults_extra_file;
our $quick_collect;
# Set to 1 if you want the tests to override
# default storage engine settings, and use MyISAM
# as default.  (temporary option used in connection
# with the change of default storage engine to InnoDB)
our $default_myisam= 0;

our $xplugin;
our $group_replication;

sub collect_option {
  my ($opt, $value)= @_;

  # Evaluate $opt as string to use "Getopt::Long::Callback legacy API"
  my $opt_name = "$opt";

  # Convert - to _ in option name
  $opt_name =~ s/-/_/g;
  no strict 'refs';
  ${$opt_name}= $value;
}

use File::Basename;
use File::Spec::Functions qw / splitdir /;
use My::File::Path qw / get_bld_path /;
use IO::File();
use My::Config;
use My::Platform;
use My::Test;
use My::Find;
use My::SysInfo;

require "mtr_misc.pl";

# Precompiled regex's for tests to do or skip
my $do_test_reg;
my $skip_test_reg;

# Related to adding InnoDB plugin combinations
my $lib_innodb_plugin;
my $do_innodb_plugin;

# If "Quick collect", set to 1 once a test to run has been found.
my $some_test_found;

sub init_pattern {
  my ($from, $what)= @_;
  return undef unless defined $from;
  if ( $from =~ /^[a-z0-9\.]*$/ ) {
    # Does not contain any regex (except . that we allow as
    # separator betwen suite and testname), make the pattern match
    # beginning of string
    $from= "^$from";
    mtr_verbose("$what='$from'");
  }
  # Check that pattern is a valid regex
  eval { "" =~/$from/; 1 } or
    mtr_error("Invalid regex '$from' passed to $what\nPerl says: $@");
  return $from;
}


##############################################################################
#
#  Collect information about test cases to be run
#
##############################################################################

sub collect_test_cases ($$$$) {
  my $opt_reorder= shift; # True if we're reordering tests
  my $suites= shift; # Semicolon separated list of test suites
  my $opt_cases= shift;
  my $opt_skip_test_list= shift;
  my $cases= []; # Array of hash(one hash for each testcase)

  # Unit tests off by default also if using --do-test or --start-from
  $::opt_ctest= 0 if $::opt_ctest == -1 && ($do_test || $start_from);

  $do_test_reg= init_pattern($do_test, "--do-test");
  $skip_test_reg= init_pattern($skip_test, "--skip-test");

  $lib_innodb_plugin=
    my_find_file($::basedir,
		 ["storage/innodb_plugin", "storage/innodb_plugin/.libs",
		  "lib/mysql/plugin", "lib/plugin"],
		 ["ha_innodb_plugin.dll", "ha_innodb_plugin.so",
		  "ha_innodb_plugin.sl"],
		 NOT_REQUIRED);
  $do_innodb_plugin= ($::mysql_version_id >= 50100 &&
		      !(IS_WINDOWS) &&
		      $lib_innodb_plugin);

  # If not reordering, we also shouldn't group by suites, unless
  # no test cases were named.
  # This also effects some logic in the loop following this.
  if ($opt_reorder or !@$opt_cases)
  {
    my $parallel = $ENV{NUMBER_OF_CPUS};
    $parallel = $::opt_parallel if ($::opt_parallel < $parallel);
    $parallel = 1 if $quick_collect;
    $parallel = 1 if @$opt_cases;

    if ($parallel == 1 or !$threads_support or !$threads_shared_support)
    {
      foreach my $suite (split(",", $suites))
      {
        push(@$cases, collect_one_suite($suite, $opt_cases,
                                        $opt_skip_test_list));
        last if $some_test_found;
        push(@$cases, collect_one_suite("i_".$suite, $opt_cases,
                                        $opt_skip_test_list));
      }
    }
    else
    {
      share(\$xplugin);
      share(\$group_replication);
      share(\$some_test_found);
      # Array containing thread id of all the threads used for
      # collecting test cases from different test suites.
      my @collect_test_cases_thrds;

      foreach my $suite (split(",", $suites))
      {
        push(@collect_test_cases_thrds, threads->create("collect_one_suite",
                                                        $suite, $opt_cases,
                                                        $opt_skip_test_list));
        while($parallel <= scalar @collect_test_cases_thrds)
        {
          mtr_milli_sleep(100);
          @collect_test_cases_thrds= threads->list(threads::running());
        }
        last if $some_test_found;

        push(@collect_test_cases_thrds, threads->create("collect_one_suite",
                                                        "i_".$suite, $opt_cases,
                                                        $opt_skip_test_list));
        while($parallel <= scalar @collect_test_cases_thrds)
        {
          mtr_milli_sleep(100);
          @collect_test_cases_thrds= threads->list(threads::running());
        }
      }

      foreach my $collect_thrd(threads->list())
      {
        my @suite_cases= $collect_thrd->join();
        push (@$cases, @suite_cases);
      }
    }
  }

  if ( @$opt_cases )
  {
    # A list of tests was specified on the command line.
    # Among those, the tests which are not already collected will be
    # collected and stored temporarily in an array of hashes pointed
    # by the below reference. This array is eventually appeneded to
    # the one having all collected test cases.
    my $cmdline_cases;

    # Check that the tests specified were found
    # in at least one suite
    foreach my $test_name_spec ( @$opt_cases )
    {
      my $found= 0;
      my ($sname, $tname, $extension)= split_testname($test_name_spec);
      foreach my $test ( @$cases )
      {
	last unless $opt_reorder;
	# test->{name} is always in suite.name format
	if ( $test->{name} =~ /^$sname.*\.$tname$/ )
	{
	  $found= 1;
	  last;
	}
      }

      if ( not $found )
      {
        if ( $sname )
        {
          # If suite was part of name, find it there, may come with combinations
          my @this_case = collect_one_suite($sname, [ $tname ]);

          # If a test is specified multiple times on the command line, all
          # instances of the test need to be picked. Hence, such tests are
          # stored in the temporary array instead of adding them to $cases
          # directly so that repeated tests are not run only once
          if ( @this_case )
          {
            push (@$cmdline_cases, @this_case);
          }
          else
          {
	    mtr_error("Could not find '$tname' in '$sname' suite");
          }
        }
        else
        {
          if ( !$opt_reorder )
          {
            # If --no-reorder is passed and if suite was not part of name,
            # search in all the suites
            foreach my $suite (split(",", $suites))
            {
              my @this_case = collect_one_suite($suite, [ $tname ]);
              if ( @this_case )
              {
                push (@$cmdline_cases, @this_case);
                $found= 1;
              }
              @this_case= collect_one_suite("i_".$suite, [ $tname ]);
              if ( @this_case )
              {
                push (@$cmdline_cases, @this_case);
                $found= 1;
              }
            }
          }
          if ( !$found )
          {
            mtr_error("Could not find '$tname' in '$suites' suite(s)");
          }
        }
      }
    }
    # Add test cases collected in the temporary array to the one
    # containing all previously collected test cases
    push (@$cases, @$cmdline_cases) if $cmdline_cases;
  }

  if ( $opt_reorder && !$quick_collect)
  {
    # Reorder the test cases in an order that will make them faster to run
    # Make a mapping of test name to a string that represents how that test
    # should be sorted among the other tests.  Put the most important criterion
    # first, then a sub-criterion, then sub-sub-criterion, etc.
    foreach my $tinfo (@$cases)
    {
      my @criteria = ();

      #
      # Append the criteria for sorting, in order of importance.
      #
      push(@criteria, "ndb=" . ($tinfo->{'ndb_test'} ? "A" : "B"));
      push(@criteria, $tinfo->{template_path});
      # Group test with equal options together.
      # Ending with "~" makes empty sort later than filled
      my $opts= $tinfo->{'master_opt'} ? $tinfo->{'master_opt'} : [];
      push(@criteria, join("!", sort @{$opts}) . "~");
      # Add slave opts if any
      if ($tinfo->{'slave_opt'})
      {
	push(@criteria, join("!", sort @{$tinfo->{'slave_opt'}}));
      }
      # This sorts tests with force-restart *before* identical tests
      push(@criteria, $tinfo->{force_restart} ? "force-restart" : "no-restart");

      $tinfo->{criteria}= join(" ", @criteria);
    }

    @$cases = sort {$a->{criteria} cmp $b->{criteria}; } @$cases;
  }

  # When $opt_repeat > 1 and $opt_parallel > 1, duplicate each test
  # $opt_repeat number of times to allow them running in parallel.
  if ($::opt_repeat > 1 and $::opt_parallel > 1) {
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

  my $new_tests;
  foreach my $test (@$tests) {
    # Don't repeat the test if 'skip' flag is enabled.
    if ($test->{'skip'}) {
      push(@{$new_tests}, $test);
    } else {
      for (my $i = 1; $i <= $::opt_repeat; $i++) {
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
      push(@{$new_test->{$key}}, @$value);
    } else {
      $new_test->{$key} = $value;
    }
  }

  return $new_test;
}

# Returns (suitename, testname, extension)
sub split_testname {
  my ($test_name)= @_;

  # If .test file name is used, get rid of directory part
  $test_name= basename($test_name) if $test_name =~ /\.test$/;

  # Now split name on .'s
  my @parts= split(/\./, $test_name);

  if (@parts == 1){
    # Only testname given, ex: alias
    return (undef , $parts[0], undef);
  } elsif (@parts == 2) {
    # Either testname.test or suite.testname given
    # Ex. main.alias or alias.test

    if ($parts[1] eq "test")
    {
      return (undef , $parts[0], $parts[1]);
    }
    else
    {
      return ($parts[0], $parts[1], undef);
    }

  } elsif (@parts == 3) {
    # Fully specified suitename.testname.test
    # ex main.alias.test
    return ( $parts[0], $parts[1], $parts[2]);
  }

  mtr_error("Illegal format of test name: $test_name");
}


sub collect_one_suite($)
{
  my $suite= shift;  # Test suite name
  my $opt_cases= shift;
  my $opt_skip_test_list= shift;
  my @cases; # Array of hash

  mtr_verbose("Collecting: $suite");

  # Default suite(i.e main suite) directory location
  my $suitedir= "$::glob_mysql_test_dir";

  if ( $suite ne "main" )
  {
    # Allow suite to be path to "some dir" if $suite has at least
    # one directory part
    if ( -d $suite and splitdir($suite) > 1 ){
      $suitedir= $suite;
      mtr_report(" - from '$suitedir'");

    }
    else
    {
      $suitedir= my_find_dir($::basedir,
			     ["share/mysql-test/suite",
			      "mysql-test/suite",
                              "lib/mysql-test/suite",
			      "internal/mysql-test/suite",
			      "mysql-test",
			      # Look in storage engine specific suite dirs
			      "storage/*/mtr",
			      # Look in plugin specific suite dir
			      "plugin/$suite/tests",
			      "internal/plugin/$suite/tests",
			      "rapid/plugin/$suite/tests",
			      "rapid/mysql-test/suite",
                              "components/$suite/tests",
			     ],
			     [$suite, "mtr"], ($suite =~ /^i_/));
      return unless $suitedir;
    }
    mtr_verbose("suitedir: $suitedir");
  }

  my $testdir= "$suitedir/t";
  my $resdir=  "$suitedir/r";

  # Check if t/ exists
  if (-d $testdir){
    # t/ exists

    if ( -d $resdir )
    {
      # r/exists
    }
    else
    {
      # No r/, use t/ as result dir
      $resdir= $testdir;
    }

  }
  else {
    # No t/ dir => there can' be any r/ dir
    mtr_error("Can't have r/ dir without t/") if -d $resdir;

    # No t/ or r/ => use suitedir
    $resdir= $testdir= $suitedir;
  }

  mtr_verbose("testdir: $testdir");
  mtr_verbose("resdir: $resdir");

  # ----------------------------------------------------------------------
  # Build a hash of disabled testcases for this suite
  # ----------------------------------------------------------------------
  my %disabled;
  foreach my $skip_file(@{$opt_skip_test_list})
  {
    $skip_file= get_bld_path($skip_file);
  }
  my @disabled_collection= @{$opt_skip_test_list} if $opt_skip_test_list;
  unshift (@disabled_collection, "$testdir/disabled.def");

  # Check for the tests to be skipped in a sanitizer which are listed
  # in "mysql-test/collections/disabled-<sanitizer>.list" file.
  if ($::opt_sanitize)
  {
    # Check for disabled-asan.list
    if($::mysql_version_extra =~ /asan/i &&
       !grep(/disabled-asan\.list$/, @{$opt_skip_test_list}))
    {
      push (@disabled_collection,
            "collections/disabled-asan.list");
    }
    # Check for disabled-ubsan.list
    elsif($::mysql_version_extra =~ /ubsan/i &&
         !grep(/disabled-ubsan\.list$/, @{$opt_skip_test_list}))
    {
      push (@disabled_collection,
            "collections/disabled-ubsan.list");
    }
  }

  for my $skip (@disabled_collection)
    {
      if ( open(DISABLED, $skip ) )
	{
	  # $^O on Windows considered not generic enough
	  my $plat= (IS_WINDOWS) ? 'windows' : $^O;

	  while ( <DISABLED> )
	    {
	      chomp;
	      #diasble the test case if platform matches
	      if ( /\@/ )
		{
		  if ( /\@$plat/ )
		    {
		      /^\s*(\S+)\s*\@$plat.*:\s*(.*?)\s*$/ ;
		      $disabled{$1}= $2 if not exists $disabled{$1};
		    }
		  elsif ( /\@!(\S*)/ )
		    {
		      if ( $1 ne $plat)
			{
			  /^\s*(\S+)\s*\@!.*:\s*(.*?)\s*$/ ;
			  $disabled{$1}= $2 if not exists $disabled{$1};
			}
		    }
		}
	      elsif ( /^\s*(\S+)\s*:\s*(.*?)\s*$/ )
		{
		  chomp;
		  if ( /^\s*(\S+)\s*:\s*(.*?)\s*$/ )
		    {
		      $disabled{$1}= $2 if not exists $disabled{$1};
		    }
		}
	    }
	  close DISABLED;
	}
    }

  # Read suite.opt file
  my $suite_opt_file=  "$testdir/suite.opt";

  if ( $::opt_suite_opt )
  {
    $suite_opt_file= "$testdir/$::opt_suite_opt";
  }

  my $suite_opts= [];
  if ( -f $suite_opt_file )
  {
    $suite_opts= opts_from_file($suite_opt_file);
  }

  if ( @$opt_cases )
  {
    # Collect in specified order
    foreach my $test_name_spec ( @$opt_cases )
    {
      my ($sname, $tname, $extension)= split_testname($test_name_spec);

      # The test name parts have now been defined
      #print "  suite_name: $sname\n";
      #print "  tname:      $tname\n";
      #print "  extension:  $extension\n";

      # Check cirrect suite if suitename is defined
      next if (defined $sname and $suite ne $sname);

      if ( defined $extension )
      {
	my $full_name= "$testdir/$tname.$extension";
	# Extension was specified, check if the test exists
        if ( ! -f $full_name)
        {
	  # This is only an error if suite was specified, otherwise it
	  # could exist in another suite
          mtr_error("Test '$full_name' was not found in suite '$sname'")
	    if $sname;

	  next;
        }
      }
      else
      {
	# No extension was specified, use default
	$extension= "test";
	my $full_name= "$testdir/$tname.$extension";

	# Test not found here, could exist in other suite
	next if ( ! -f $full_name );
      }

      push(@cases,
	   collect_one_test_case($suitedir,
				 $testdir,
				 $resdir,
				 $suite,
				 $tname,
				 "$tname.$extension",
				 \%disabled,
				 $suite_opts));
    }
  }
  else
  {
    opendir(TESTDIR, $testdir) or mtr_error("Can't open dir \"$testdir\": $!");

    foreach my $elem ( sort readdir(TESTDIR) )
    {
      my $tname= mtr_match_extension($elem, 'test');

      next unless defined $tname;

      # Skip tests that does not match the --do-test= filter
      next if ($do_test_reg and not $tname =~ /$do_test_reg/o);

      push(@cases,
	   collect_one_test_case($suitedir,
				 $testdir,
				 $resdir,
				 $suite,
				 $tname,
				 $elem,
				 \%disabled,
				 $suite_opts));
    }
    closedir TESTDIR;
  }

  #  Return empty list if no testcases found
  return if (@cases == 0);

  # ----------------------------------------------------------------------
  # Read combinations for this suite and build testcases x combinations
  # if any combinations exists
  # ----------------------------------------------------------------------
  if ( ! $skip_combinations && ! $quick_collect )
  {
    my @combinations;
    my $combination_file= "$suitedir/combinations";
    #print "combination_file: $combination_file\n";
    if (@::opt_combinations)
    {
      # take the combination from command-line
      mtr_verbose("Take the combination from command line");
      foreach my $combination (@::opt_combinations) {
	my $comb= {};
	$comb->{name}= $combination;
	push(@{$comb->{comb_opt}}, $combination);
	push(@combinations, $comb);
      }
    }
    elsif (-f $combination_file )
    {
      # Read combinations file in my.cnf format
      mtr_verbose("Read combinations file");
      my $config= My::Config->new($combination_file);
      foreach my $group ($config->groups()) {
	my $comb= {};
	$comb->{name}= $group->name();
        foreach my $option ( $group->options() ) {
	  push(@{$comb->{comb_opt}}, $option->option());
	}
	push(@combinations, $comb);
      }
    }

    if (@combinations)
    {
      print " - adding combinations for $suite\n";
      #print_testcases(@cases);

      my @new_cases;
      foreach my $comb (@combinations)
      {
	foreach my $test (@cases)
	{

	  next if ( $test->{'skip'} );

	  # Skip this combination if the values it provides
	  # already are set in master_opt or slave_opt
	  if (My::Options::is_set($test->{master_opt}, $comb->{comb_opt}) ||
	      My::Options::is_set($test->{slave_opt}, $comb->{comb_opt}) ){
	    next;
	  }

	  # Copy test options
	  my $new_test= My::Test->new();
	  while (my ($key, $value) = each(%$test)) {
	    if (ref $value eq "ARRAY") {
	      push(@{$new_test->{$key}}, @$value);
	    } else {
	      $new_test->{$key}= $value;
	    }
	  }

	  # Append the combination options to master_opt and slave_opt
	  push(@{$new_test->{master_opt}}, @{$comb->{comb_opt}});
	  push(@{$new_test->{slave_opt}}, @{$comb->{comb_opt}});

	  # Add combination name short name
	  $new_test->{combination}= $comb->{name};

	  # Add the new test to new test cases list
	  push(@new_cases, $new_test);
	}
      }

      # Add the plain test if it was not already added
      # as part of a combination
      my %added;
      foreach my $new_test (@new_cases){
	$added{$new_test->{name}}= 1;
      }
      foreach my $test (@cases){
	push(@new_cases, $test) unless $added{$test->{name}};
      }


      #print_testcases(@new_cases);
      @cases= @new_cases;
      #print_testcases(@cases);
    }
  }

  optimize_cases(\@cases);
  #print_testcases(@cases);

  return @cases;
}



#
# Loop through all test cases
# - optimize which test to run by skipping unnecessary ones
# - update settings if necessary
#
sub optimize_cases {
  my ($cases)= @_;

  foreach my $tinfo ( @$cases )
  {
    # Skip processing if already marked as skipped
    next if $tinfo->{skip};

    # =======================================================
    # If a special binlog format was selected with
    # --mysqld=--binlog-format=x, skip all test that does not
    # support it
    # =======================================================
    #print "binlog_format: $binlog_format\n";
    if (defined $binlog_format )
    {
      # =======================================================
      # Fixed --binlog-format=x specified on command line
      # =======================================================
      if ( defined $tinfo->{'binlog_formats'} )
      {
	#print "binlog_formats: ". join(", ", @{$tinfo->{binlog_formats}})."\n";

	# The test supports different binlog formats
	# check if the selected one is ok
	my $supported=
	  grep { $_ eq lc $binlog_format } @{$tinfo->{'binlog_formats'}};
	if ( !$supported )
	{
	  $tinfo->{'skip'}= 1;
	  $tinfo->{'comment'}=
	    "Doesn't support --binlog-format='$binlog_format'";
	}
      }
    }
    else
    {
      # =======================================================
      # Use dynamic switching of binlog format
      # =======================================================

      # Get binlog-format used by this test from master_opt
      my $test_binlog_format;
      foreach my $opt ( @{$tinfo->{master_opt}} ) {
       (my $dash_opt = $opt) =~ s/_/-/g;
	$test_binlog_format=
	  mtr_match_prefix($dash_opt, "--binlog-format=") || $test_binlog_format;
      }

      if (defined $test_binlog_format and
	  defined $tinfo->{binlog_formats} )
      {
	my $supported=
	  grep { My::Options::option_equals($_, lc $test_binlog_format) }
            @{$tinfo->{'binlog_formats'}};
	if ( !$supported )
	{
	  $tinfo->{'skip'}= 1;
	  $tinfo->{'comment'}=
	    "Doesn't support --binlog-format='$test_binlog_format'";
	  next;
	}
      }
    }

    # =======================================================
    # Check that engine selected by
    # --default-storage-engine=<engine> is supported
    # =======================================================
    my %builtin_engines = ('myisam' => 1, 'memory' => 1, 'csv' => 1);

    foreach my $opt ( @{$tinfo->{master_opt}} ) {
     (my $dash_opt = $opt) =~ s/_/-/g;

      # Check whether server supports SSL connection
      if ($dash_opt eq "--skip-ssl" and $::opt_ssl)
      {
        $tinfo->{'skip'}= 1;
        $tinfo->{'comment'}= "Server doesn't support SSL connection";
        next;
      }

      my $default_engine=
	mtr_match_prefix($dash_opt, "--default-storage-engine=");
      my $default_tmp_engine=
	mtr_match_prefix($dash_opt, "--default-tmp-storage-engine=");

      # Allow use of uppercase, convert to all lower case
      $default_engine =~ tr/A-Z/a-z/;
      $default_tmp_engine =~ tr/A-Z/a-z/;

      if (defined $default_engine){

	#print " $tinfo->{name}\n";
	#print " - The test asked to use '$default_engine'\n";

	#my $engine_value= $::mysqld_variables{$default_engine};
	#print " - The mysqld_variables says '$engine_value'\n";

	if ( ! exists $::mysqld_variables{$default_engine} and
	     ! exists $builtin_engines{$default_engine} )
	{
	  $tinfo->{'skip'}= 1;
	  $tinfo->{'comment'}=
	    "'$default_engine' not supported";
	}

	$tinfo->{'ndb_test'}= 1
	  if ( $default_engine =~ /^ndb/i );
	$tinfo->{'myisam_test'}= 1
	  if ( $default_engine =~ /^myisam/i );
      }
      if (defined $default_tmp_engine){

	#print " $tinfo->{name}\n";
	#print " - The test asked to use '$default_tmp_engine' as temp engine\n";

	#my $engine_value= $::mysqld_variables{$default_tmp_engine};
	#print " - The mysqld_variables says '$engine_value'\n";

	if ( ! exists $::mysqld_variables{$default_tmp_engine} and
	     ! exists $builtin_engines{$default_tmp_engine} )
	{
	  $tinfo->{'skip'}= 1;
	  $tinfo->{'comment'}=
	    "'$default_tmp_engine' not supported";
	}

	$tinfo->{'ndb_test'}= 1
	  if ( $default_tmp_engine =~ /^ndb/i );
	$tinfo->{'myisam_test'}= 1
	  if ( $default_tmp_engine =~ /^myisam/i );
      }
    }

    if ($quick_collect && ! $tinfo->{'skip'})
    {
      $some_test_found= 1;
      return;
    }
  }
}


#
# Read options from the given opt file and append them as an array
# to $tinfo->{$opt_name}
#
sub process_opts_file {
  my ($tinfo, $opt_file, $opt_name)= @_;

  if ( -f $opt_file )
  {
    my $opts= opts_from_file($opt_file);

    foreach my $opt ( @$opts )
    {
      my $value;

      # The opt file is used both to send special options to the mysqld
      # as well as pass special test case specific options to this
      # script

      $value= mtr_match_prefix($opt, "--timezone=");
      if ( defined $value )
      {
	$tinfo->{'timezone'}= $value;
	next;
      }

      $value= mtr_match_prefix($opt, "--result-file=");
      if ( defined $value )
      {
	# Specifies the file mysqltest should compare
	# output against
	$tinfo->{'result_file'}= "r/$value.result";
	next;
      }

      $value= mtr_match_prefix($opt, "--config-file-template=");
      if ( defined $value)
      {
	# Specifies the configuration file to use for this test
	$tinfo->{'template_path'}= dirname($tinfo->{path})."/$value";
	next;
      }

      # If we set default time zone, remove the one we have
      $value= mtr_match_prefix($opt, "--default-time-zone=");
      if ( defined $value )
      {
	# Set timezone for this test case to something different
	$tinfo->{'timezone'}= "GMT-8";
	# Fallthrough, add the --default-time-zone option
      }

      # The --restart option forces a restart even if no special
      # option is set. If the options are the same as next testcase
      # there is no need to restart after the testcase
      # has completed
      if ( $opt eq "--force-restart" )
      {
	$tinfo->{'force_restart'}= 1;
	next;
      }

      $value= mtr_match_prefix($opt, "--testcase-timeout=");
      if ( defined $value ) {
	# Overrides test case timeout for this test
	$tinfo->{'case-timeout'}= $value;
	next;
      }

      # Ok, this was a real option, add it
      push(@{$tinfo->{$opt_name}}, $opt);
    }
  }
}

##############################################################################
#
#  Collect information about a single test case
#
##############################################################################

sub collect_one_test_case {
  my $suitedir=   shift;
  my $testdir=    shift;
  my $resdir=     shift;
  my $suitename=  shift;
  my $tname=      shift;
  my $filename=   shift;
  my $disabled=   shift;
  my $suite_opts= shift;

  # Test file name should consist of only alpha-numeric characters, dash (-)
  # or underscore (_), but should not start with dash or underscore.
  if ($tname !~ /^[^_\W][\w-]*$/)
  {
    die("Invalid test file name '$suitename.$tname'. Test file ".
        "name should consist of only alpha-numeric characters, ".
        "dash (-) or underscore (_), but should not start with ".
        "dash or underscore.");
  }

  # ----------------------------------------------------------------------
  # Check --start-from
  # ----------------------------------------------------------------------
  if ( $start_from )
  {
    # start_from can be specified as [suite.].testname_prefix
    my ($suite, $test, $ext)= split_testname($start_from);

    if ( $suite and $suitename lt $suite){
      return; # Skip silently
    }
    if ( $tname lt $test ){
      return; # Skip silently
    }
  }

  # ----------------------------------------------------------------------
  # Set defaults
  # ----------------------------------------------------------------------
  my $tinfo= My::Test->new
    (
     name          => "$suitename.$tname",
     shortname     => $tname,
     path          => "$testdir/$filename",

    );

  my $result_file= "$resdir/$tname.result";
  if (-f $result_file) {
    # Allow nonexistsing result file
    # in that case .test must issue "exit" otherwise test
    # should fail by default
    $tinfo->{result_file}= $result_file;
  }
  else
  {
    # Result file doesn't exist
    if ($::opt_check_testcases and !$::opt_record)
    {
      # Set 'no_result_file' flag if check-testcases is enabled.
      $tinfo->{'no_result_file'}= $result_file;
    }
    else
    {
      # No .result file exist, remember the path where it should
      # be saved in case of --record.
      $tinfo->{record_file}= $result_file;
    }
  }

  # ----------------------------------------------------------------------
  # Skip some tests but include in list, just mark them as skipped
  # ----------------------------------------------------------------------
  if ( $skip_test_reg and $tname =~ /$skip_test_reg/o )
  {
    $tinfo->{'skip'}= 1;
    return $tinfo;
  }

  # ----------------------------------------------------------------------
  # Check for replicaton tests
  # ----------------------------------------------------------------------
  $tinfo->{'rpl_test'}= 1 if ($suitename =~ 'rpl');
  $tinfo->{'rpl_nogtid_test'}= 1 if($suitename =~ 'rpl_nogtid');
  $tinfo->{'rpl_gtid_test'}= 1 if ($suitename =~ 'rpl_gtid');
  $tinfo->{'grp_rpl_test'}= 1 if ($suitename =~ 'group_replication');

  # ----------------------------------------------------------------------
  # Check for disabled tests
  # ----------------------------------------------------------------------
  my $marked_as_disabled= 0;
  if ( $disabled->{$tname} or $disabled->{"$suitename.$tname"} )
  {
    # Test was marked as disabled in suites disabled.def file
    $marked_as_disabled= 1;
    # Test name may have been disabled with or without suite name part
    $tinfo->{'comment'}= $disabled->{$tname} ||
                         $disabled->{"$suitename.$tname"};
  }

  my $disabled_file= "$testdir/$tname.disabled";
  if ( -f $disabled_file )
  {
    $marked_as_disabled= 1;
    $tinfo->{'comment'}= mtr_fromfile($disabled_file);
  }

  if ($marked_as_disabled) {
    if ($enable_disabled or @::opt_cases) {
      # User has selected to run all disabled tests
      mtr_report(" - Running test $tinfo->{name} even though it's been",
                 "disabled due to '$tinfo->{comment}'.");
    } else {
      $tinfo->{'skip'}= 1;
      # Disable the test case
      $tinfo->{'disable'}= 1;
      return $tinfo;
    }
  }

  # ----------------------------------------------------------------------
  # Append suite extra options to both master and slave
  # ----------------------------------------------------------------------
  push(@{$tinfo->{'master_opt'}}, @$suite_opts);
  push(@{$tinfo->{'slave_opt'}}, @$suite_opts);

  #-----------------------------------------------------------------------
  # Check for test specific config file
  # ----------------------------------------------------------------------
  my $test_cnf_file= "$testdir/$tname.cnf";
  if ( -f $test_cnf_file ) {
    # Specifies the configuration file to use for this test
    $tinfo->{'template_path'}= $test_cnf_file;
  }

  # ----------------------------------------------------------------------
  # master sh
  # ----------------------------------------------------------------------
  my $master_sh= "$testdir/$tname-master.sh";
  if ( -f $master_sh )
  {
    if ( IS_WIN32PERL )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No tests with sh scripts on Windows";
      return $tinfo;
    }
    else
    {
      $tinfo->{'master_sh'}= $master_sh;
    }
  }

  # ----------------------------------------------------------------------
  # slave sh
  # ----------------------------------------------------------------------
  my $slave_sh= "$testdir/$tname-slave.sh";
  if ( -f $slave_sh )
  {
    if ( IS_WIN32PERL )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No tests with sh scripts on Windows";
      return $tinfo;
    }
    else
    {
      $tinfo->{'slave_sh'}= $slave_sh;
    }
  }

  # ----------------------------------------------------------------------
  # <tname>.slave-mi
  # ----------------------------------------------------------------------
  mtr_error("$tname: slave-mi not supported anymore")
    if ( -f "$testdir/$tname.slave-mi");


  tags_from_test_file($tinfo,"$testdir/${tname}.test");

  # Disable the result file check for NDB tests not having its
  # corresponding result file.
  if ($tinfo->{'ndb_test'} and $tinfo->{'ndb_no_result_file_test'})
  {
    delete $tinfo->{'no_result_file'} if $tinfo->{'no_result_file'};
  }

  if ( defined $default_storage_engine )
  {
    # Different default engine is used
    # tag test to require that engine
    $tinfo->{'ndb_test'}= 1
      if ( $default_storage_engine =~ /^ndb/i );

    $tinfo->{'mysiam_test'}= 1
      if ( $default_storage_engine =~ /^mysiam/i );
  }

  # Skip non-parallel tests if 'non-parallel-test' option is disabled
  if ($tinfo->{'not_parallel'} and !$::opt_non_parallel_test)
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'comment'}= "Test needs 'non-parallel-test' option";
    return $tinfo;
  }

  # Except the tests which need big-test or only-big-test option to run
  # in valgrind environment(i.e tests having no_valgrind_without_big.inc
  # include file), other normal/non-big tests shouldn't run with
  # only-big-test option.
  if ($::opt_only_big_test)
  {
    if (!$tinfo->{'no_valgrind_without_big'} and !$tinfo->{'big_test'})
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Not a big test";
      return $tinfo;
    }
  }

  # Check for big test
  if ($tinfo->{'big_test'} and !($::opt_big_test or $::opt_only_big_test))
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'comment'}= "Test needs 'big-test' or 'only-big-test' option";
    return $tinfo;
  }

  # Tests having no_valgrind_without_big.inc include file needs either
  # big-test or only-big-test option to run in valgrind environment.
  if ($tinfo->{'no_valgrind_without_big'} and $::opt_valgrind)
  {
    if (!$::opt_big_test and !$::opt_only_big_test)
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Need '--big-test' or '--only-big-test' when ".
                           "running with Valgrind.";
      return $tinfo;
    }
  }

  if ( $tinfo->{'need_debug'} && ! $::debug_compiled_binaries )
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'comment'}= "Test needs debug binaries";
    return $tinfo;
  }

  if ( $tinfo->{'ndb_test'} )
  {
    # This is a NDB test
    if ( $::ndbcluster_enabled == 0)
    {
      # ndbcluster is disabled
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "ndbcluster disabled";
      return $tinfo;
    }
  }
  else
  {
    # This is not a ndb test
    if ( $opt_with_ndbcluster_only )
    {
      # Only the ndb test should be run, all other should be skipped
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Only ndbcluster tests";
      return $tinfo;
    }
  }

  if ($tinfo->{'federated_test'})
  {
    # This is a test that needs federated, enable it
    push(@{$tinfo->{'master_opt'}}, "--loose-federated");
    push(@{$tinfo->{'slave_opt'}}, "--loose-federated");
  }
  if ( $tinfo->{'myisam_test'})
  {
    # This is a temporary fix to allow non-innodb tests to run even if
    # the default storage engine is innodb.
    push(@{$tinfo->{'master_opt'}}, "--default-storage-engine=MyISAM");
    push(@{$tinfo->{'slave_opt'}}, "--default-storage-engine=MyISAM");
    push(@{$tinfo->{'master_opt'}}, "--default-tmp-storage-engine=MyISAM");
    push(@{$tinfo->{'slave_opt'}}, "--default-tmp-storage-engine=MyISAM");
  }
  if ( $tinfo->{'need_binlog'} )
  {
    if (grep(/^--skip[-_]log[-_]bin/,  @::opt_extra_mysqld_opt) )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test needs binlog";
      return $tinfo;
    }
  }
  else
  {
    # Test does not need binlog, add --skip-binlog to
    # the options used when starting
    # push(@{$tinfo->{'master_opt'}}, "--loose-skip-log-bin");
    # push(@{$tinfo->{'slave_opt'}}, "--loose-skip-log-bin");
  }

  if ( $tinfo->{'rpl_test'} or $tinfo->{'grp_rpl_test'} )
  {
    if ( $skip_rpl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No replication tests(--skip-rpl)";
      return $tinfo;
    }
  }

  if ( $tinfo->{'need_ssl'} )
  {
    # This is a test that needs ssl
    if ( ! $::opt_ssl_supported ) {
      # SSL is not supported, skip it
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No SSL support";
      return $tinfo;
    }
  }

  # Check for group replication tests
  $group_replication= 1 if ($tinfo->{'grp_rpl_test'});

  # Check for xplugin tests
  $xplugin= 1 if ($tinfo->{'xplugin_test'});

  if ( $tinfo->{'not_windows'} && IS_WINDOWS )
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'comment'}= "Test not supported on Windows";
    return $tinfo;
  }

  # ----------------------------------------------------------------------
  # Find config file to use if not already selected in <testname>.opt file
  # ----------------------------------------------------------------------
  if (defined $defaults_file) {
    # Using same config file for all tests
    $defaults_file= get_bld_path($defaults_file);
    $tinfo->{template_path}= $defaults_file;
  }
  elsif (! $tinfo->{template_path} )
  {
    my $config= "$suitedir/my.cnf";
    if (! -f $config )
    {
      # assume default.cnf will be used
      $config= "include/default_my.cnf";

      # rpl_gtid tests must use their suite's cnf file having gtid mode on.
      if ( $tinfo->{rpl_gtid_test} )
      {
        $config= "suite/rpl_gtid/my.cnf";
      }
      # rpl_nogtid tests must use their suite's cnf file having gtid mode off.
      elsif ( $tinfo->{rpl_nogtid_test} )
      {
        $config= "suite/rpl_nogtid/my.cnf";
      }
      # rpl tests must use their suite's cnf file.
      elsif ( $tinfo->{rpl_test} )
      {
        $config= "suite/rpl/my.cnf";
      }

      # ndb tests must use their suite specific cnf files.
      if ( $tinfo->{ndb_test} )
      {
        if ( $tinfo->{rpl_test} )
        {
          $config= "suite/rpl_ndb/my.cnf";
        }
        else
        {
          $config= "suite/ndb/my.cnf";
        }
      }
    }
    $tinfo->{template_path}= $config;
  }

  # Set extra config file to use
  if (defined $defaults_extra_file) {
    $defaults_extra_file= get_bld_path($defaults_extra_file);
    $tinfo->{extra_template_path}= $defaults_extra_file;
  }

  # ----------------------------------------------------------------------
  # Append mysqld extra options to both master and slave
  # ----------------------------------------------------------------------
  push(@{$tinfo->{'master_opt'}}, @::opt_extra_mysqld_opt);
  push(@{$tinfo->{'slave_opt'}}, @::opt_extra_mysqld_opt);

  if ( !$::start_only or @::opt_cases )
  {
    # ----------------------------------------------------------------------
    # Add master opts, extra options only for master
    # ----------------------------------------------------------------------
    process_opts_file($tinfo, "$testdir/$tname-master.opt", 'master_opt');

    # ----------------------------------------------------------------------
    # Add slave opts, list of extra option only for slave
    # ----------------------------------------------------------------------
    process_opts_file($tinfo, "$testdir/$tname-slave.opt", 'slave_opt');
  }

  if (!$::start_only)
  {
    # ----------------------------------------------------------------------
    # Add client opts, extra options only for mysqltest client
    # ----------------------------------------------------------------------
    process_opts_file($tinfo, "$testdir/$tname-client.opt", 'client_opt');
  }

  return $tinfo;
}


# List of tags in the .test files that if found should set
# the specified value in "tinfo"
my @tags=
(
 ["include/have_binlog_format_row.inc", "binlog_formats", ["row"]],
 ["include/have_binlog_format_statement.inc", "binlog_formats", ["statement"]],
 ["include/have_binlog_format_mixed.inc", "binlog_formats", ["mixed", "mix"]],
 ["include/have_binlog_format_mixed_or_row.inc",
  "binlog_formats", ["mixed", "mix", "row"]],
 ["include/have_binlog_format_mixed_or_statement.inc",
  "binlog_formats", ["mixed", "mix", "statement"]],
 ["include/have_binlog_format_row_or_statement.inc",
  "binlog_formats", ["row", "statement"]],

 ["include/have_log_bin.inc", "need_binlog", 1],

 # An empty file to use test that needs myisam engine.
 ["include/force_myisam_default.inc", "myisam_test", 1],

 ["include/big_test.inc", "big_test", 1],
 ["include/have_debug.inc", "need_debug", 1],
 ["include/have_ndb.inc", "ndb_test", 1],
 ["include/have_multi_ndb.inc", "ndb_test", 1],

 # Any test sourcing the below inc file is considered to be an NDB
 # test not having its corresponding result file.
 ["include/ndb_no_result_file.inc", "ndb_no_result_file_test", 1],

 # The tests with below four .inc files are considered to be rpl tests.
 ["include/rpl_init.inc", "rpl_test", 1],
 ["include/rpl_ip_mix.inc", "rpl_test", 1],
 ["include/rpl_ip_mix2.inc", "rpl_test", 1],
 ["include/rpl_ipv6.inc", "rpl_test", 1],

 ["include/ndb_master-slave.inc", "ndb_test", 1],
 ["federated.inc", "federated_test", 1],
 ["include/have_ssl.inc", "need_ssl", 1],
 ["include/not_windows.inc", "not_windows", 1],
 ["include/not_parallel.inc", "not_parallel", 1],

 # Tests with below .inc file are considered to be group replication tests
 ["have_group_replication_plugin_base.inc", "grp_rpl_test", 1],
 ["have_group_replication_plugin.inc", "grp_rpl_test", 1],

 # Tests with below .inc file are considered to be xplugin tests
 ["include/have_mysqlx_plugin.inc", "xplugin_test", 1],

 # Tests with below .inc file needs either big-test or only-big-test
 # option along with valgrind option.
 ["include/no_valgrind_without_big.inc", "no_valgrind_without_big", 1],
);


sub tags_from_test_file {
  my $tinfo= shift;
  my $file= shift;
  #mtr_verbose("$file");
  my $F= IO::File->new($file) or mtr_error("can't open file \"$file\": $!");

  while ( my $line= <$F> )
  {

    # Skip line if it start's with #
    next if ( $line =~ /^#/ );

    # Match this line against tag in "tags" array
    foreach my $tag (@tags)
    {
      if ( index($line, $tag->[0]) >= 0 )
      {
	# Tag matched, assign value to "tinfo"
	$tinfo->{"$tag->[1]"}= $tag->[2];
      }
    }

    # If test sources another file, open it as well
    if ( $line =~ /^\-\-([[:space:]]*)source(.*)$/ or
	 $line =~ /^([[:space:]]*)source(.*);$/ )
    {
      my $value= $2;
      $value =~ s/^\s+//;  # Remove leading space
      $value =~ s/[[:space:]]+$//;  # Remove ending space

      # Sourced file may exist relative to test or
      # in global location
      foreach my $sourced_file (dirname($file). "/$value",
				"$::glob_mysql_test_dir/$value")
      {
	if ( -f $sourced_file )
	{
	  # Only source the file if it exists, we may get
	  # false positives in the regexes above if someone
	  # writes "source nnnn;" in a test case(such as mysqltest.test)
	  tags_from_test_file($tinfo, $sourced_file);
	  last;
	}
      }
    }

  }
}

sub unspace {
  my $string= shift;
  my $quote=  shift;
  $string =~ s/[ \t]/\x11/g;
  return "$quote$string$quote";
}


sub opts_from_file ($) {
  my $file=  shift;

  open(FILE,"<",$file) or mtr_error("can't open file \"$file\": $!");
  my @args;
  while ( <FILE> )
  {
    chomp;

    #    --init_connect=set @a='a\\0c'
    s/^\s+//;                           # Remove leading space
    s/\s+$//;                           # Remove ending space

    # This is strange, but we need to fill whitespace inside
    # quotes with something, to remove later. We do this to
    # be able to split on space. Else, we have trouble with
    # options like
    #
    #   --someopt="--insideopt1 --insideopt2"
    #
    # But still with this, we are not 100% sure it is right,
    # we need a shell to do it right.

    s/\'([^\'\"]*)\'/unspace($1,"\x0a")/ge;
    s/\"([^\'\"]*)\"/unspace($1,"\x0b")/ge;
    s/\'([^\'\"]*)\'/unspace($1,"\x0a")/ge;
    s/\"([^\'\"]*)\"/unspace($1,"\x0b")/ge;

    foreach my $arg (split(/[ \t]+/))
    {
      $arg =~ tr/\x11\x0a\x0b/ \'\"/;     # Put back real chars
      # The outermost quotes has to go
      $arg =~ s/^([^\'\"]*)\'(.*)\'([^\'\"]*)$/$1$2$3/
        or $arg =~ s/^([^\'\"]*)\"(.*)\"([^\'\"]*)$/$1$2$3/;
      $arg =~ s/\\\\/\\/g;

      # Do not pass empty string since my_getopt is not capable to handle it.
      if (length($arg)) {
	push(@args, $arg);
      }
    }
  }
  close FILE;
  return \@args;
}

sub print_testcases {
  my (@cases)= @_;

  print "=" x 60, "\n";
  foreach my $test (@cases){
    $test->print_test();
  }
  print "=" x 60, "\n";
}


1;
