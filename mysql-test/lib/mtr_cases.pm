# -*- cperl -*-
# Copyright (c) 2005, 2011, Oracle and/or its affiliates.
# Copyright (c) 2010, 2011 Monty Program Ab
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

package mtr_cases;
use strict;

use base qw(Exporter);
our @EXPORT= qw(collect_option collect_test_cases);

use mtr_report;
use mtr_match;

# Options used for the collect phase
our $start_from;
our $skip_rpl;
our $do_test;
our $skip_test;
our $binlog_format;
our $enable_disabled;
our $default_storage_engine;
our $opt_with_ndbcluster_only;
our $defaults_file;

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
use File::Spec::Functions qw /splitdir/;
use IO::File();
use My::Config;
use My::Platform;
use My::Test;
use My::Find;

require "mtr_misc.pl";

# Precompiled regex's for tests to do or skip
my $do_test_reg;
my $skip_test_reg;

my %suites;
my $default_suite_object = do 'My/Suite.pm';

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

  $do_test_reg= init_pattern($do_test, "--do-test");
  $skip_test_reg= init_pattern($skip_test, "--skip-test");

  # If not reordering, we also shouldn't group by suites, unless
  # no test cases were named.
  # This also effects some logic in the loop following this.
  if ($opt_reorder or !@$opt_cases)
  {
    foreach my $suite (split(",", $suites))
    {
      push(@$cases, collect_one_suite($suite, $opt_cases, $opt_skip_test_list));
    }
  }

  if ( @$opt_cases )
  {
    # A list of tests was specified on the command line
    # Check that the tests specified was found
    # in at least one suite
    foreach my $test_name_spec ( @$opt_cases )
    {
      my $found= 0;
      my ($sname, $tname)= split_testname($test_name_spec);
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
	$sname= "main" if !$opt_reorder and !$sname;
	mtr_error("Could not find '$tname' in '$suites' suite(s)") unless $sname;
	# If suite was part of name, find it there, may come with combinations
	my @this_case = collect_one_suite($sname, [ $tname ]);
	if (@this_case)
        {
	  push (@$cases, @this_case);
	}
	else
	{
	  mtr_error("Could not find '$tname' in '$sname' suite");
        }
      }
    }
  }

  if ( $opt_reorder )
  {
    # Reorder the test cases in an order that will make them faster to run
    my %sort_criteria;

    # Make a mapping of test name to a string that represents how that test
    # should be sorted among the other tests.  Put the most important criterion
    # first, then a sub-criterion, then sub-sub-criterion, etc.
    foreach my $tinfo (@$cases)
    {
      my @criteria = ();

      #
      # Append the criteria for sorting, in order of importance.
      #
      push @criteria, ($tinfo->{'long_test'} ? "long" : "short");
      push(@criteria, $tinfo->{template_path});
      for (qw(master_opt slave_opt)) {
        # Group test with equal options together.
        # Ending with "~" makes empty sort later than filled
        my $opts= $tinfo->{$_} ? $tinfo->{$_} : [];
        push(@criteria, join("!", sort @{$opts}) . "~");
      }
      push @criteria, $tinfo->{name};
      $tinfo->{criteria}= join(" ", @criteria);
    }

    @$cases = sort { $a->{criteria} cmp $b->{criteria} } @$cases;
  }

  return $cases;
}


# Returns (suitename, testname)
sub split_testname {
  my ($test_name)= @_;

  # If .test file name is used, get rid of directory part
  $test_name= basename($test_name) if $test_name =~ /\.test$/;

  # Now split name on .'s
  my @parts= split(/\./, $test_name);

  if (@parts == 1){
    # Only testname given, ex: alias
    return (undef , $parts[0]);
  } elsif (@parts == 2) {
    # Either testname.test or suite.testname given
    # Ex. main.alias or alias.test

    if ($parts[1] eq "test")
    {
      return (undef , $parts[0]);
    }
    else
    {
      return ($parts[0], $parts[1]);
    }
  }

  mtr_error("Illegal format of test name: $test_name");
}


sub collect_one_suite
{
  my $suite= shift;  # Test suite name
  my $opt_cases= shift;
  my $opt_skip_test_list= shift;
  my @cases; # Array of hash

  mtr_verbose("Collecting: $suite");

  my $suitedir= "$::glob_mysql_test_dir"; # Default
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
			      "share/mysql/mysql-test/suite",
			      "mysql-test/suite",
			      "mysql-test",
			      # Look in storage engine specific suite dirs
			      "storage/*/mtr",
			      # Look in plugin specific suite dir
			      "plugin/$suite/tests",
			     ],
			     [$suite, "mtr"]);
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

  #
  # Load the Suite object
  #
  unless ($suites{$suite}) {
    if (-f "$suitedir/suite.pm") {
      $suites{$suite} = do "$suitedir/suite.pm";
    } else {
      $suites{$suite} = $default_suite_object;
    }
  }

  # ----------------------------------------------------------------------
  # Build a hash of disabled testcases for this suite
  # ----------------------------------------------------------------------
  my %disabled;
  my @disabled_collection= @{$opt_skip_test_list} if defined @{$opt_skip_test_list};
  unshift (@disabled_collection, "$testdir/disabled.def");
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
  my $suite_opts= [ opts_from_file("$testdir/suite.opt") ];
  $suite_opts = [ opts_from_file("$suitedir/suite.opt") ] unless @$suite_opts;

  my @case_names;
  {
    my $s= $suites{$suite};
    $s = 'My::Suite' unless ref $s;
    @case_names= $s->list_cases($testdir);
  }

  if ( @$opt_cases )
  {
    my (%case_names)= map { $_ => 1 } @case_names;
    @case_names= ();

    # Collect in specified order
    foreach my $test_name_spec ( @$opt_cases )
    {
      my ($sname, $tname)= split_testname($test_name_spec);

      # Check correct suite if suitename is defined
      next if (defined $sname and $suite ne $sname);

      # Extension was specified, check if the test exists
      if ( ! $case_names{$tname})
      {
        # This is only an error if suite was specified, otherwise it
        # could exist in another suite
        mtr_error("Test '$tname' was not found in suite '$sname'")
          if $sname;

        next;
      }
      push @case_names, $tname;
    }
  }

  foreach (@case_names)
  {
    # Skip tests that do not match the --do-test= filter
    next if ($do_test_reg and not $_ =~ /$do_test_reg/o);

    push(@cases, collect_one_test_case($suitedir, $testdir, $resdir,
                                       $suite, $_, "$_.test", \%disabled,
                                       $suite_opts));
  }

  #  Return empty list if no testcases found
  return if (@cases == 0);

  # ----------------------------------------------------------------------
  # Read combinations for this suite and build testcases x combinations
  # if any combinations exists
  # ----------------------------------------------------------------------
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
      my %env_filter = map { $_ => 1 } split /:/, $ENV{"\U${suite}_COMBINATIONS"};
      my $config= My::Config->new($combination_file);
      foreach my $group ($config->groups()) {
	my $comb= {};
	$comb->{name}= $group->name();
        next if %env_filter and not $env_filter{$comb->{name}};
        foreach my $option ( $group->options() ) {
	  push(@{$comb->{comb_opt}}, $option->option());
	}
	push(@combinations, $comb) if $comb->{comb_opt};
      }
    }

    if (@combinations)
    {
      print " - adding combinations for $suite\n";

      my @new_cases;
      TEST: foreach my $test (@cases)
      {
        if ( $test->{'skip'} )
        {
          push(@new_cases, $test);
          next;
        }

        foreach my $comb (@combinations)
        {
	  # Skip all other combinations if the values they change
	  # are already fixed in master_opt or slave_opt
	  if (My::Options::is_set($test->{master_opt}, $comb->{comb_opt}) &&
	      My::Options::is_set($test->{slave_opt}, $comb->{comb_opt}) ){

            # Add combination name short name
            $test->{combination}= $comb->{name};

            # Add the test to new test cases list
            push(@new_cases, $test);
            next TEST;
	  }
        }

        foreach my $comb (@combinations)
        {
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

      @cases= @new_cases;
    }
  }

  optimize_cases(\@cases);

  return @cases;
}



#
# Loop through all test cases
# - optimize which test to run by skipping unnecessary ones
# - update settings if necessary
#
sub optimize_cases {
  my ($cases)= @_;

  my @new_cases= ();

  foreach my $tinfo ( @$cases )
  {
    push @new_cases, $tinfo;

    # Skip processing if already marked as skipped
    next if $tinfo->{skip};

    # =======================================================
    # If a special binlog format was selected with
    # --mysqld=--binlog-format=x, skip all test that does not
    # support it
    # =======================================================
    #print "binlog_format: $binlog_format\n";
    if (not defined $binlog_format )
    {
      # =======================================================
      # Use dynamic switching of binlog format
      # =======================================================

      # Get binlog-format used by this test from master_opt
      my $test_binlog_format;
      foreach my $opt ( @{$tinfo->{master_opt}} ) {
	$test_binlog_format=
	  mtr_match_prefix($opt, "--binlog-format=") || $test_binlog_format;
      }

      if (defined $test_binlog_format and
	  defined $tinfo->{binlog_formats} )
      {
	my $supported=
	  grep { $_ eq $test_binlog_format } @{$tinfo->{'binlog_formats'}};
	if ( !$supported )
	{
	  $tinfo->{'skip'}= 1;
	  $tinfo->{'comment'}=
	    "Doesn't support --binlog-format='$test_binlog_format'";
          # This test was added as a replication combination, but it is not
          # actually ever possible to run it, as it is not made for this
          # combination.
          # So delete it from the list, rather than confuse the user with a
          # message that this test is skipped (it is not really, just run
          # with other combinations).
          pop(@new_cases);
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
      my $default_engine=
	mtr_match_prefix($opt, "--default-storage-engine=");

      # Allow use of uppercase, convert to all lower case
      $default_engine =~ tr/A-Z/a-z/;

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
      }
    }
  }
  @$cases= @new_cases;
}


#
# Read options from the given opt file and append them as an array
# to $tinfo->{$opt_name}
#
sub process_opts {
  my ($tinfo, $opt_name)= @_;

  my @opts= @{$tinfo->{$opt_name}};
  $tinfo->{$opt_name} = [];

  foreach my $opt (@opts)
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

    # If we set default time zone, remove the one we have
    $value= mtr_match_prefix($opt, "--default-time-zone=");
    if ( defined $value )
    {
      # Set timezone for this test case to something different
      $tinfo->{'timezone'}= "GMT-8";
      # Fallthrough, add the --default-time-zone option
    }

    # Ok, this was a real option, add it
    push(@{$tinfo->{$opt_name}}, $opt);
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

  my $local_default_storage_engine= $default_storage_engine;

  # ----------------------------------------------------------------------
  # Set defaults
  # ----------------------------------------------------------------------
  my $tinfo= My::Test->new
    (
     name          => "$suitename.$tname",
     shortname     => $tname,
     path          => "$testdir/$filename",
     suite         => $suites{$suitename},
    );

  my $result_file= "$resdir/$tname.result";
  if (-f $result_file) {
    # Allow nonexistsing result file
    # in that case .test must issue "exit" otherwise test
    # should fail by default
    $tinfo->{result_file}= $result_file;
  }
  else {
    # No .result file exist
    # Remember the path  where it should be
    # saved in case of --record
    $tinfo->{record_file}= $result_file;
  }

  # ----------------------------------------------------------------------
  # Skip some tests but include in list, just mark them as skipped
  # ----------------------------------------------------------------------
  my $name= basename($suitename) . ".$tname";
  if ( $skip_test_reg and ($tname =~ /$skip_test_reg/o ||
                           $name =~ /$skip_test/o))
  {
    $tinfo->{'skip'}= 1;
    return $tinfo;
  }

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

  if ( $marked_as_disabled )
  {
    if ( $enable_disabled )
    {
      # User has selected to run all disabled tests
      mtr_report(" - $tinfo->{name} wil be run although it's been disabled\n",
		 "  due to '$tinfo->{comment}'");
    }
    else
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'disable'}= 1;   # Sub type of 'skip'
      return $tinfo;
    }
  }

  # ----------------------------------------------------------------------
  # Append suite extra options to both master and slave
  # ----------------------------------------------------------------------
  push(@{$tinfo->{'master_opt'}}, @$suite_opts);
  push(@{$tinfo->{'slave_opt'}}, @$suite_opts);

  # ----------------------------------------------------------------------
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

  my ($master_opts, $slave_opts)=
    tags_from_test_file($tinfo, "$testdir/${tname}.test", $suitedir);

  # Get default storage engine from suite.opt file

  if (defined $suite_opts &&
      "@$suite_opts" =~ "default-storage-engine=\s*([^\s]*)")
  {
    $local_default_storage_engine= $1;
  }

  if ( defined $local_default_storage_engine )
  {
    # Different default engine is used
    # tag test to require that engine
    $tinfo->{'ndb_test'}= 1
      if ( $local_default_storage_engine =~ /^ndb/i );

  }

  if ( $tinfo->{'big_test'} and ! $::opt_big_test )
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'comment'}= "Test needs --big-test";
    return $tinfo
  }

  if ( $tinfo->{'big_test'} )
  {
    # All 'big_test' takes a long time to run
    $tinfo->{'long_test'}= 1;
  }

  if ( ! $tinfo->{'big_test'} and $::opt_big_test > 1 )
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'comment'}= "Small test";
    return $tinfo
  }

  if ( $tinfo->{'ndb_test'} )
  {
    # This is a NDB test
    if ( $::opt_skip_ndbcluster == 2 )
    {
      # Ndb is not supported, skip it
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No ndbcluster support or ndb tests not enabled";
      return $tinfo;
    }
    elsif ( $::opt_skip_ndbcluster )
    {
      # All ndb test's should be skipped
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No ndbcluster";
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

  if ( $tinfo->{'rpl_test'} )
  {
    if ( $skip_rpl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No replication tests";
      return $tinfo;
    }
  }

  if ( $tinfo->{'need_ipv6'} )
  {
    # This is a test that needs ssl
    if ( ! $::have_ipv6 ) {
      # IPv6 is not supported, skip it
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No IPv6";
      return $tinfo;
    }
  }

  # ----------------------------------------------------------------------
  # Find config file to use if not already selected in <testname>.opt file
  # ----------------------------------------------------------------------
  if (defined $defaults_file) {
    # Using same config file for all tests
    $tinfo->{template_path}= $defaults_file;
  }
  elsif (! $tinfo->{template_path} )
  {
    my $config= "$suitedir/my.cnf";
    if (! -f $config )
    {
      # assume default.cnf will be used
      $config= "include/default_my.cnf";

      # Suite has no config, autodetect which one to use
      if ( $tinfo->{rpl_test} ){
	$config= "suite/rpl/my.cnf";
	if ( $tinfo->{ndb_test} ){
	  $config= "suite/rpl_ndb/my.cnf";
	}
      }
      elsif ( $tinfo->{ndb_test} ){
	$config= "suite/ndb/my.cnf";
      }
    }
    $tinfo->{template_path}= $config;
  }

  if (not ref $suites{$suitename})
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'comment'}= $suites{$suitename};
    return $tinfo;
  }

  # ----------------------------------------------------------------------
  # Append mysqld extra options to master and slave, as appropriate
  # ----------------------------------------------------------------------
  push @{$tinfo->{'master_opt'}}, @$master_opts, @::opt_extra_mysqld_opt;
  push @{$tinfo->{'slave_opt'}}, @$slave_opts, @::opt_extra_mysqld_opt;

  process_opts($tinfo, 'master_opt');
  process_opts($tinfo, 'slave_opt');

  return $tinfo;
}


my $tags_map= {'big_test' => ['big_test', 1],
               'have_ndb' => ['ndb_test', 1],
               'have_multi_ndb' => ['ndb_test', 1],
               'master-slave' => ['rpl_test', 1],
               'ndb_master-slave' => ['rpl_test', 1, 'ndb_test', 1],
               'check_ipv6' => ['need_ipv6', 1],
               'long_test' => ['long_test', 1],
};
my $tags_regex_string= join('|', keys %$tags_map);
my $tags_regex= qr:include/($tags_regex_string)\.inc:o;

my $file_to_tags= { };
my $file_to_master_opts= { };
my $file_to_slave_opts= { };

# Get various tags from a file, recursively scanning also included files.
# And get options from .opt file, also recursively for included files.
# Return a list of [TAG_TO_SET, VALUE_TO_SET_TO] of found tags.
# Also returns lists of options for master and slave found in .opt files.
# Each include file is scanned only once, and subsequent calls just look up the
# cached result.
# We need to be a bit careful about speed here; previous version of this code
# took forever to scan the full test suite.
sub get_tags_from_file {
  my ($file, $suitedir)= @_;

  return ([], [], []) unless -f $file;

  return ($file_to_tags->{$file}, $file_to_master_opts->{$file},
          $file_to_slave_opts->{$file})
    if exists($file_to_tags->{$file});

  my $F= IO::File->new($file)
    or mtr_error("can't open file \"$file\": $!");

  my $tags= [];
  my $master_opts= [];
  my $slave_opts= [];

  while (my $line= <$F>)
  {
    # Ignore comments.
    next if $line =~ /^\#/;

    # Add any tag we find.
    if ($line =~ /$tags_regex/o)
    {
      my $to_set= $tags_map->{$1};
      for (my $i= 0; $i < @$to_set; $i+= 2)
      {
        push @$tags, [$to_set->[$i], $to_set->[$i+1]];
      }
    }

    # Check for a sourced include file.
    if ($line =~ /^(--)?[[:space:]]*source[[:space:]]+([^;[:space:]]+)/)
    {
      my $include= $2;
      # Sourced file may exist relative to test file, or in global location.
      # Note that for the purpose of tag collection we ignore
      # non-existing files, and let mysqltest handle the error
      # (e.g. mysqltest.test needs this)
      for my $sourced_file (dirname($file) . '/' . $include,
                            $suitedir . '/' . $include,
                            $::glob_mysql_test_dir . '/' . $include)
      {
        if (-e $sourced_file)
        {
          my ($sub_tags, $sub_master_opts, $sub_slave_opts)=
            get_tags_from_file($sourced_file, $suitedir);
          push @$tags, @$sub_tags;
          push @$master_opts, @$sub_master_opts;
          push @$slave_opts, @$sub_slave_opts;
          last;
        }
      }
    }
  }

  # Add options from main file _after_ those of any includes; this allows a
  # test file to override options set by includes (eg. rpl.rpl_ddl uses this
  # to enable innodb, then disable innodb in the slave.
  my $file_no_ext= $file;
  $file_no_ext =~ s/\.\w+$//;
  my @common_opts= opts_from_file("$file_no_ext.opt");
  push @$master_opts, @common_opts, opts_from_file("$file_no_ext-master.opt");
  push @$slave_opts, @common_opts, opts_from_file("$file_no_ext-slave.opt");

  # Save results so we can reuse without parsing if seen again.
  $file_to_tags->{$file}= $tags;
  $file_to_master_opts->{$file}= $master_opts;
  $file_to_slave_opts->{$file}= $slave_opts;
  return ($tags, $master_opts, $slave_opts);
}

sub tags_from_test_file {
  my ($tinfo, $file, $suitedir)= @_;

  my ($tags, $master_opts, $slave_opts)= get_tags_from_file($file, $suitedir);
  for (@$tags)
  {
    $tinfo->{$_->[0]}= $_->[1];
  }
  return ($master_opts, $slave_opts);
}

sub unspace {
  my $string= shift;
  my $quote=  shift;
  $string =~ s/[ \t]/\x11/g;
  return "$quote$string$quote";
}


sub opts_from_file ($) {
  my $file=  shift;
  local $_;

  return () unless -f $file;

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
  return @args;
}

1;

