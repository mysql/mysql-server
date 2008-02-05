# -*- cperl -*-
# Copyright (C) 2005-2006 MySQL AB
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

use File::Basename;
use IO::File();
use strict;

use My::Config;

sub collect_test_cases ($);
sub collect_one_suite ($);
sub collect_one_test_case ($$$$$$$$$);

sub mtr_options_from_test_file($$);

my $do_test;
my $skip_test;

sub init_pattern {
  my ($from, $what)= @_;
  if ( $from =~ /^[a-z0-9]$/ ) {
    # Does not contain any regex, make the pattern match
    # beginning of string
    $from= "^$from";
  }
  # Check that pattern is a valid regex
  eval { "" =~/$from/; 1 } or
    mtr_error("Invalid regex '$from' passed to $what\nPerl says: $@");
  return $from;
}



##############################################################################
#
#  Collect information about test cases we are to run
#
##############################################################################

sub collect_test_cases ($) {
  $do_test= init_pattern($::opt_do_test, "--do-test");
  $skip_test= init_pattern($::opt_skip_test, "--skip-test");

  my $suites= shift; # Semicolon separated list of test suites
  my $cases = [];    # Array of hash

  foreach my $suite (split(",", $suites))
  {
    push(@$cases, collect_one_suite($suite));
  }


  if ( @::opt_cases )
  {
    # Check that the tests specified was found
    # in at least one suite
    foreach my $test_name_spec ( @::opt_cases )
    {
      my $found= 0;
      my ($sname, $tname, $extension)= split_testname($test_name_spec);
      foreach my $test ( @$cases )
      {
	# test->{name} is always in suite.name format
	if ( $test->{name} =~ /.*\.$tname/ )
	{
	  $found= 1;
	}
      }
      if ( not $found )
      {
	mtr_error("Could not find $tname in any suite");
      }
    }
  }

  if ( $::opt_reorder )
  {
    # Reorder the test cases in an order that will make them faster to run
    my %sort_criteria;

    # Make a mapping of test name to a string that represents how that test
    # should be sorted among the other tests.  Put the most important criterion
    # first, then a sub-criterion, then sub-sub-criterion, et c.
    foreach my $tinfo (@$cases)
    {
      my @criteria = ();

      # Look for tests that muct be in run in a defined order
      # that is defined by test having the same name except for
      # the ending digit

      # Put variables into hash
      my $test_name= $tinfo->{'name'};
      my $depend_on_test_name;
      if ( $test_name =~ /^([\D]+)([0-9]{1})$/ )
      {
	my $base_name= $1;
	my $idx= $2;
	mtr_verbose("$test_name =>  $base_name idx=$idx");
	if ( $idx > 1 )
	{
	  $idx-= 1;
	  $base_name= "$base_name$idx";
	  mtr_verbose("New basename $base_name");
	}

	foreach my $tinfo2 (@$cases)
	{
	  if ( $tinfo2->{'name'} eq $base_name )
	  {
	    mtr_verbose("found dependent test $tinfo2->{'name'}");
	    $depend_on_test_name=$base_name;
	  }
	}
      }

      if ( defined $depend_on_test_name )
      {
	mtr_verbose("Giving $test_name same critera as $depend_on_test_name");
	$sort_criteria{$test_name} = $sort_criteria{$depend_on_test_name};
      }
      else
      {
	#
	# Append the criteria for sorting, in order of importance.
	#
	push(@criteria, "ndb=" . ($tinfo->{'ndb_test'} ? "1" : "0"));
	# Group test with equal options together.
	# Ending with "~" makes empty sort later than filled
	push(@criteria, join("!", sort @{$tinfo->{'master_opt'}}) . "~");

	$sort_criteria{$test_name} = join(" ", @criteria);
      }
    }

    @$cases = sort {
      $sort_criteria{$a->{'name'}} . $a->{'name'} cmp
	$sort_criteria{$b->{'name'}} . $b->{'name'}; } @$cases;

    if ( $::opt_script_debug )
    {
      # For debugging the sort-order
      foreach my $tinfo (@$cases)
      {
	print("$sort_criteria{$tinfo->{'name'}} -> \t$tinfo->{'name'}\n");
      }
    }
  }

  return $cases;

}

# Valid extensions and their corresonding component id
my %exts = ( 'test' => 'mysqld',
	     'imtest' => 'im'
	   );


# Returns (suitename, testname, extension)
sub split_testname {
  my ($test_name)= @_;

  # Get rid of directory part and split name on .'s
  my @parts= split(/\./, basename($test_name));

  if (@parts == 1){
    # Only testname given, ex: alias
    return (undef , $parts[0], undef);
  } elsif (@parts == 2) {
    # Either testname.test or suite.testname given
    # Ex. main.alias or alias.test

    if (defined $exts{$parts[1]})
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
  my @cases;  # Array of hash

  mtr_verbose("Collecting: $suite");

  my $suitedir= "$::glob_mysql_test_dir"; # Default
  if ( $suite ne "main" )
  {
    $suitedir= mtr_path_exists("$suitedir/suite/$suite",
			       "$suitedir/$suite");
    mtr_verbose("suitedir: $suitedir");
  }

  my $testdir= "$suitedir/t";
  my $resdir=  "$suitedir/r";

  # ----------------------------------------------------------------------
  # Build a hash of disabled testcases for this suite
  # ----------------------------------------------------------------------
  my %disabled;
  if ( open(DISABLED, "$testdir/disabled.def" ) )
  {
    while ( <DISABLED> )
      {
        chomp;
        if ( /^\s*(\S+)\s*:\s*(.*?)\s*$/ )
          {
            $disabled{$1}= $2;
          }
      }
    close DISABLED;
  }

  # Read suite.opt file
  my $suite_opt_file=  "$testdir/suite.opt";
  my $suite_opts= [];
  if ( -f $suite_opt_file )
  {
    $suite_opts= mtr_get_opts_from_file($suite_opt_file);
  }

  if ( @::opt_cases )
  {
    # Collect in specified order
    foreach my $test_name_spec ( @::opt_cases )
    {
      my ($sname, $tname, $extension)= split_testname($test_name_spec);

      # The test name parts have now been defined
      #print "  suite_name: $sname\n";
      #print "  tname:      $tname\n";
      #print "  extension:  $extension\n";

      # Check cirrect suite if suitename is defined
      next if (defined $sname and $suite ne $sname);

      my $component_id;
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
	$component_id= $exts{$extension};
      }
      else
      {
	# No extension was specified
	my ($ext, $component);
	while (($ext, $component)= each %exts) {
	  my $full_name= "$testdir/$tname.$ext";

	  if ( ! -f $full_name ) {
	    next;
	  }
	  $component_id= $component;
	  $extension= $ext;
	}
	# Test not found here, could exist in other suite
	next unless $component_id;
      }

      collect_one_test_case($testdir,$resdir,$suite,$tname,
                            "$tname.$extension",\@cases,\%disabled,
			    $component_id,$suite_opts);
    }
  }
  else
  {
    opendir(TESTDIR, $testdir) or mtr_error("Can't open dir \"$testdir\": $!");

    foreach my $elem ( sort readdir(TESTDIR) )
    {
      my $component_id= undef;
      my $tname= undef;

      if ($tname= mtr_match_extension($elem, 'test'))
      {
        $component_id = 'mysqld';
      }
      elsif ($tname= mtr_match_extension($elem, 'imtest'))
      {
        $component_id = 'im';
      }
      else
      {
        next;
      }

      # Skip tests that does not match the --do-test= filter
      next if ($do_test and not $tname =~ /$do_test/o);

      collect_one_test_case($testdir,$resdir,$suite,$tname,
                            $elem,\@cases,\%disabled,$component_id,
			    $suite_opts);
    }
    closedir TESTDIR;
  }


  #  Return empty list if no testcases found
  return if (@cases == 0);

  # ----------------------------------------------------------------------
  # Read combinations for this suite and build testcases x combinations
  # if any combinations exists
  # ----------------------------------------------------------------------
  if ( ! $::opt_skip_combination )
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
	  push(@{$comb->{comb_opt}}, $option->name()."=".$option->value());
	}
	push(@combinations, $comb);
      }
    }

    if (@combinations)
    {
      print " - adding combinations\n";
      #print_testcases(@cases);

      my @new_cases;
      foreach my $comb (@combinations)
      {
	foreach my $test (@cases)
	{
	  #print $test->{name}, " ", $comb, "\n";
	  my $new_test= {};

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

	  # Add combination name shrt name
	  $new_test->{combination}= $comb->{name};

	  # Add the new test to new test cases list
	  push(@new_cases, $new_test);
	}
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

    # Replication test needs an adjustment of binlog format
    if (mtr_match_prefix($tinfo->{'name'}, "rpl"))
    {

      # =======================================================
      # Get binlog-format used by this test from master_opt
      # =======================================================
      my $test_binlog_format;
      foreach my $opt ( @{$tinfo->{master_opt}} ) {
	$test_binlog_format= $test_binlog_format ||
	  mtr_match_prefix($opt, "--binlog-format=");
      }
      # print $tinfo->{name}." uses ".$test_binlog_format."\n";

      # =======================================================
      # If a special binlog format was selected with
      # --mysqld=--binlog-format=x, skip all test with different
      # binlog-format
      # =======================================================
      if (defined $::used_binlog_format and
	  $test_binlog_format and
	  $::used_binlog_format ne $test_binlog_format)
      {
	$tinfo->{'skip'}= 1;
	$tinfo->{'comment'}= "Requires --binlog-format='$test_binlog_format'";
	next;
      }

      # =======================================================
      # Check that testcase supports the designated binlog-format
      # =======================================================
      if ($test_binlog_format and defined $tinfo->{'sup_binlog_formats'} )
      {
	my $supported=
	  grep { $_ eq $test_binlog_format } @{$tinfo->{'sup_binlog_formats'}};
	if ( !$supported )
	{
	  $tinfo->{'skip'}= 1;
	  $tinfo->{'comment'}=
	    "Doesn't support --binlog-format='$test_binlog_format'";
	  next;
	}
      }

      # =======================================================
      # Use dynamic switching of binlog-format if mtr started
      # w/o --mysqld=--binlog-format=xxx and combinations.
      # =======================================================
      if (!defined $tinfo->{'combination'} and
          !defined $::used_binlog_format)
      {
        $test_binlog_format= $tinfo->{'sup_binlog_formats'}->[0];
      }

      # Save binlog format for dynamic switching
      $tinfo->{binlog_format}= $test_binlog_format;
    }
  }
}


##############################################################################
#
#  Collect information about a single test case
#
##############################################################################


sub collect_one_test_case($$$$$$$$$) {
  my $testdir= shift;
  my $resdir=  shift;
  my $suite=   shift;
  my $tname=   shift;
  my $elem=    shift;
  my $cases=   shift;
  my $disabled=shift;
  my $component_id= shift;
  my $suite_opts= shift;

  my $path= "$testdir/$elem";

  # ----------------------------------------------------------------------
  # Skip some tests silently
  # ----------------------------------------------------------------------

  if ( $::opt_start_from and $tname lt $::opt_start_from )
  {
    return;
  }


  my $tinfo= {};
  $tinfo->{'name'}= basename($suite) . ".$tname";
  $tinfo->{'result_file'}= "$resdir/$tname.result";
  $tinfo->{'component_id'} = $component_id;
  push(@$cases, $tinfo);

  # ----------------------------------------------------------------------
  # Skip some tests but include in list, just mark them to skip
  # ----------------------------------------------------------------------

  if ( $skip_test and $tname =~ /$skip_test/o )
  {
    $tinfo->{'skip'}= 1;
    return;
  }

  # ----------------------------------------------------------------------
  # Collect information about test case
  # ----------------------------------------------------------------------

  $tinfo->{'path'}= $path;
  $tinfo->{'timezone'}= "GMT-3"; # for UNIX_TIMESTAMP tests to work

  $tinfo->{'slave_num'}= 0; # Default, no slave
  $tinfo->{'master_num'}= 1; # Default, 1 master
  if ( defined mtr_match_prefix($tname,"rpl") )
  {
    if ( $::opt_skip_rpl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No replication tests(--skip-rpl)";
      return;
    }

    $tinfo->{'slave_num'}= 1; # Default for rpl* tests, use one slave

  }

  if ( defined mtr_match_prefix($tname,"federated") )
  {
    # Default, federated uses the first slave as it's federated database
    $tinfo->{'slave_num'}= 1;
  }

  my $master_opt_file= "$testdir/$tname-master.opt";
  my $slave_opt_file=  "$testdir/$tname-slave.opt";
  my $slave_mi_file=   "$testdir/$tname.slave-mi";
  my $master_sh=       "$testdir/$tname-master.sh";
  my $slave_sh=        "$testdir/$tname-slave.sh";
  my $disabled_file=   "$testdir/$tname.disabled";
  my $im_opt_file=     "$testdir/$tname-im.opt";

  $tinfo->{'master_opt'}= [];
  $tinfo->{'slave_opt'}=  [];
  $tinfo->{'slave_mi'}=   [];


  # Add suite opts
  foreach my $opt ( @$suite_opts )
  {
    mtr_verbose($opt);
    push(@{$tinfo->{'master_opt'}}, $opt);
    push(@{$tinfo->{'slave_opt'}}, $opt);
  }

  # Add master opts
  if ( -f $master_opt_file )
  {

    my $master_opt= mtr_get_opts_from_file($master_opt_file);

    foreach my $opt ( @$master_opt )
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

      $value= mtr_match_prefix($opt, "--slave-num=");
      if ( defined $value )
      {
	$tinfo->{'slave_num'}= $value;
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

      # Ok, this was a real option, add it
      push(@{$tinfo->{'master_opt'}}, $opt);
    }
  }

  # Add slave opts
  if ( -f $slave_opt_file )
  {
    my $slave_opt= mtr_get_opts_from_file($slave_opt_file);

    foreach my $opt ( @$slave_opt )
    {
      # If we set default time zone, remove the one we have
      my $value= mtr_match_prefix($opt, "--default-time-zone=");
      $tinfo->{'slave_opt'}= [] if defined $value;
    }
    push(@{$tinfo->{'slave_opt'}}, @$slave_opt);
  }

  if ( -f $slave_mi_file )
  {
    $tinfo->{'slave_mi'}= mtr_get_opts_from_file($slave_mi_file);
  }

  if ( -f $master_sh )
  {
    if ( $::glob_win32_perl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No tests with sh scripts on Windows";
      return;
    }
    else
    {
      $tinfo->{'master_sh'}= $master_sh;
    }
  }

  if ( -f $slave_sh )
  {
    if ( $::glob_win32_perl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No tests with sh scripts on Windows";
      return;
    }
    else
    {
      $tinfo->{'slave_sh'}= $slave_sh;
    }
  }

  if ( -f $im_opt_file )
  {
    $tinfo->{'im_opts'} = mtr_get_opts_from_file($im_opt_file);
  }
  else
  {
    $tinfo->{'im_opts'} = [];
  }

  # FIXME why this late?
  my $marked_as_disabled= 0;
  if ( $disabled->{$tname} )
  {
    $marked_as_disabled= 1;
    $tinfo->{'comment'}= $disabled->{$tname};
  }

  if ( -f $disabled_file )
  {
    $marked_as_disabled= 1;
    $tinfo->{'comment'}= mtr_fromfile($disabled_file);
  }

  # If test was marked as disabled, either opt_enable_disabled is off and then
  # we skip this test, or it is on and then we run this test but warn

  if ( $marked_as_disabled )
  {
    if ( $::opt_enable_disabled )
    {
      $tinfo->{'dont_skip_though_disabled'}= 1;
    }
    else
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'disable'}= 1;   # Sub type of 'skip'
      return;
    }
  }

  if ( $component_id eq 'im' )
  {
    if ( $::glob_use_embedded_server )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No IM with embedded server";
      return;
    }
    elsif ( $::opt_ps_protocol )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No IM with --ps-protocol";
      return;
    }
    elsif ( $::opt_skip_im )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No IM tests(--skip-im)";
      return;
    }
  }
  else
  {
    mtr_options_from_test_file($tinfo,"$testdir/${tname}.test");

    if ( defined $::used_default_engine )
    {
      # Different default engine is used
      # tag test to require that engine
      $tinfo->{'ndb_test'}= 1
	if ( $::used_default_engine =~ /^ndb/i );

      $tinfo->{'innodb_test'}= 1
	if ( $::used_default_engine =~ /^innodb/i );
    }

    if ( $tinfo->{'big_test'} and ! $::opt_big_test )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test need 'big-test' option";
      return;
    }

    if ( $tinfo->{'ndb_extra'} and ! $::opt_ndb_extra_test )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test need 'ndb_extra' option";
      return;
    }

    if ( $tinfo->{'require_manager'} )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test need the _old_ manager(to be removed)";
      return;
    }

    if ( $tinfo->{'need_debug'} && ! $::debug_compiled_binaries )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test need debug binaries";
      return;
    }

    if ( $tinfo->{'ndb_test'} )
    {
      # This is a NDB test
      if ( ! $::glob_ndbcluster_supported )
      {
	# Ndb is not supported, skip it
	$tinfo->{'skip'}= 1;
	$tinfo->{'comment'}= "No ndbcluster support";
	return;
      }
      elsif ( $::opt_skip_ndbcluster )
      {
	# All ndb test's should be skipped
	$tinfo->{'skip'}= 1;
	$tinfo->{'comment'}= "No ndbcluster tests(--skip-ndbcluster)";
	return;
      }
      # Ndb tests run with two mysqld masters
      $tinfo->{'master_num'}= 2;
    }
    else
    {
      # This is not a ndb test
      if ( $::opt_with_ndbcluster_only )
      {
	# Only the ndb test should be run, all other should be skipped
	$tinfo->{'skip'}= 1;
	$tinfo->{'comment'}= "Only ndbcluster tests(--with-ndbcluster-only)";
	return;
      }
    }

    if ( $tinfo->{'innodb_test'} )
    {
      # This is a test that need innodb
      if ( $::mysqld_variables{'innodb'} ne "TRUE" )
      {
	# innodb is not supported, skip it
	$tinfo->{'skip'}= 1;
	$tinfo->{'comment'}= "No innodb support";
	return;
      }
    }

    if ( $tinfo->{'need_binlog'} )
    {
      if (grep(/^--skip-log-bin/,  @::opt_extra_mysqld_opt) )
      {
	$tinfo->{'skip'}= 1;
	$tinfo->{'comment'}= "Test need binlog";
	return;
      }
    }
    else
    {
      if ( $::mysql_version_id >= 50100 )
      {
	# Test does not need binlog, add --skip-binlog to
	# the options used when starting it
	push(@{$tinfo->{'master_opt'}}, "--skip-log-bin");
      }
    }

  }
}


# List of tags in the .test files that if found should set
# the specified value in "tinfo"
our @tags=
(
 ["include/have_innodb.inc", "innodb_test", 1],
 ["include/have_binlog_format_row.inc", "sup_binlog_formats", ["row"]],
 ["include/have_log_bin.inc", "need_binlog", 1],
 ["include/have_binlog_format_statement.inc",
  "sup_binlog_formats", ["statement"]],
 ["include/have_binlog_format_mixed.inc", "sup_binlog_formats", ["mixed"]],
 ["include/have_binlog_format_mixed_or_row.inc",
  "sup_binlog_formats", ["mixed","row"]],
 ["include/have_binlog_format_mixed_or_statement.inc",
  "sup_binlog_formats", ["mixed","statement"]],
 ["include/have_binlog_format_row_or_statement.inc",
  "sup_binlog_formats", ["row","statement"]],
 ["include/big_test.inc", "big_test", 1],
 ["include/have_debug.inc", "need_debug", 1],
 ["include/have_ndb.inc", "ndb_test", 1],
 ["include/have_multi_ndb.inc", "ndb_test", 1],
 ["include/have_ndb_extra.inc", "ndb_extra", 1],
 ["require_manager", "require_manager", 1],
);

sub mtr_options_from_test_file($$) {
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

      my $sourced_file= "$::glob_mysql_test_dir/$value";
      if ( -f $sourced_file )
      {
	# Only source the file if it exists, we may get
	# false positives in the regexes above if someone
	# writes "source nnnn;" in a test case(such as mysqltest.test)
	mtr_options_from_test_file($tinfo, $sourced_file);
      }
    }
  }
}


sub print_testcases {
  my (@cases)= @_;

  print "=" x 60, "\n";
  foreach my $test (@cases){
    print "[", $test->{name}, "]", "\n";
    while ((my ($key, $value)) = each(%$test)) {
      print " ", $key, "=";
      if (ref $value eq "ARRAY") {
	print join(", ", @$value);
      } else {
	print $value;
      }
      print "\n";
    }
    print "\n";
  }
  print "=" x 60, "\n";
}


1;
