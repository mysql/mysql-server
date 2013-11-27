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
our @EXPORT= qw(collect_option collect_test_cases collect_default_suites);

use Carp;

use mtr_report;
use mtr_match;

# Options used for the collect phase
our $skip_rpl;
our $do_test;
our $skip_test;
our $binlog_format;
our $enable_disabled;
our $default_storage_engine;
our $opt_with_ndbcluster_only;

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
use My::Suite;

require "mtr_misc.pl";

# locate plugin suites, depending on whether it's a build tree or installed
my @plugin_suitedirs;
my $plugin_suitedir_regex;
my $overlay_regex;

if (-d '../sql') {
  @plugin_suitedirs= ('storage/*/mysql-test', 'plugin/*/mysql-test');
  $overlay_regex= '\b(?:storage|plugin)/(\w+)/mysql-test\b';
} else {
  @plugin_suitedirs= ('mysql-test/plugin/*');
  $overlay_regex= '\bmysql-test/plugin/(\w+)\b';
}
$plugin_suitedir_regex= $overlay_regex;
$plugin_suitedir_regex=~ s/\Q(\w+)\E/\\w+/;

# Precompiled regex's for tests to do or skip
my $do_test_reg;
my $skip_test_reg;

my %suites;

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

  parse_disabled($_) for @$opt_skip_test_list;

  # If not reordering, we also shouldn't group by suites, unless
  # no test cases were named.
  # This also affects some logic in the loop following this.
  if ($opt_reorder or !@$opt_cases)
  {
    foreach my $suite (split(",", $suites))
    {
      push(@$cases, collect_suite_name($suite, $opt_cases));
    }
  }

  if ( @$opt_cases )
  {
    # A list of tests was specified on the command line
    # Check that the tests specified were found
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
	my @this_case = collect_suite_name($sname, [ $test_name_spec ]);
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
    # Make a mapping of test name to a string that represents how that test
    # should be sorted among the other tests.  Put the most important criterion
    # first, then a sub-criterion, then sub-sub-criterion, etc.
    foreach my $tinfo (@$cases)
    {
      my @criteria = ();

      #
      # Collect the criteria for sorting, in order of importance.
      # Note that criteria are also used in mysql-test-run.pl to
      # schedule tests to workers, and it preferres tests that have
      # *identical* criteria. That is, test name is *not* part of
      # the criteria, but it's part of the sorting function below.
      #
      push(@criteria, $tinfo->{template_path});
      for (qw(master_opt slave_opt)) {
        # Group test with equal options together.
        # Ending with "~" makes empty sort later than filled
        my $opts= $tinfo->{$_} ? $tinfo->{$_} : [];
        push(@criteria, join("!", sort @{$opts}) . "~");
      }
      $tinfo->{criteria}= join(" ", @criteria);
    }

    @$cases = sort {                            # ORDER BY
      $b->{skip} <=> $a->{skip}           ||    #   skipped DESC,
      $a->{criteria} cmp $b->{criteria}   ||    #   criteria ASC,
      $b->{long_test} <=> $a->{long_test} ||    #   long_test DESC,
      $a->{name} cmp $b->{name}                 #   name ASC
    } @$cases;
  }

  return $cases;
}


# Returns (suitename, testname, combinations....)
sub split_testname {
  my ($test_name)= @_;

  # If .test file name is used, get rid of directory part
  $test_name= basename($test_name) if $test_name =~ /\.test$/;

  # Then, get the combinations:
  my ($test_name, @combs) = split /,/, $test_name;

  # Now split name on .'s
  my @parts= split(/\./, $test_name);

  if (@parts == 1){
    # Only testname given, ex: alias
    return (undef , $parts[0], @combs);
  } elsif (@parts == 2) {
    # Either testname.test or suite.testname given
    # Ex. main.alias or alias.test

    if ($parts[1] eq "test")
    {
      return (undef , $parts[0], @combs);
    }
    else
    {
      return ($parts[0], $parts[1], @combs);
    }
  }

  mtr_error("Illegal format of test name: $test_name");
}

our %file_to_tags;
our %file_to_master_opts;
our %file_to_slave_opts;
our %file_combinations;
our %skip_combinations;
our %file_in_overlay;

sub load_suite_object {
  my ($suitename, $suitedir) = @_;
  my $suite;
  unless (defined $suites{$suitename}) {
    if (-f "$suitedir/suite.pm") {
      $suite= do "$suitedir/suite.pm";
      unless (ref $suite) {
        my $comment = $suite;
        $suite = My::Suite->new();
        $suite->{skip} = $comment;
      }
    } else {
      $suite = My::Suite->new();
    }

    $suites{$suitename} = $suite;

    # add suite skiplist to a global hash, so that we can check it
    # with only one lookup
    my %suite_skiplist = $suite->skip_combinations();
    while (my ($file, $skiplist) = each %suite_skiplist) {
      $file =~ s/\.\w+$/\.combinations/;
      if (ref $skiplist) {
        $skip_combinations{"$suitedir/$file => $_"} = 1 for (@$skiplist);
      } else {
        $skip_combinations{"$suitedir/$file"} = $skiplist;
      }
    }
  }
  return $suites{$suitename};
}


# returns a pair of (suite, suitedir)
sub suite_for_file($) {
  my ($file) = @_;
  return ($2, $1) if $file =~ m@^(.*/$plugin_suitedir_regex/(\w+))/@o;
  return ($2, $1) if $file =~ m@^(.*/mysql-test/suite/(\w+))/@;
  return ('main', $1) if $file =~ m@^(.*/mysql-test)/@;
  mtr_error("Cannot determine suite for $file");
}

sub combinations_from_file($$)
{
  my ($in_overlay, $filename) = @_;
  my @combs;
  if ($skip_combinations{$filename}) {
    @combs = ({ skip => $skip_combinations{$filename} });
  } else {
    return () if @::opt_combinations or not -f $filename;
    # Read combinations file in my.cnf format
    mtr_verbose("Read combinations file");
    my $config= My::Config->new($filename);
    foreach my $group ($config->option_groups()) {
      my $comb= { name => $group->name(), comb_opt => [] };
      next if $skip_combinations{"$filename => $comb->{name}"};
      foreach my $option ( $group->options() ) {
        push(@{$comb->{comb_opt}}, $option->option());
      }
      $comb->{in_overlay} = 1 if $in_overlay;
      push @combs, $comb;
    }
    @combs = ({ skip => 'Requires: ' . basename($filename, '.combinations') }) unless @combs;
  }
  @combs;
}

our %disabled;
sub parse_disabled {
  my ($filename, $suitename) = @_;

  if (open(DISABLED, $filename)) {
    while (<DISABLED>) {
      chomp;
      next if /^\s*#/ or /^\s*$/;
      mtr_error("Syntax error in $filename line $.")
        unless /^\s*(?:([-0-9A-Za-z_]+)\.)?([-0-9A-Za-z_]+)\s*:\s*(.*?)\s*$/;
      mtr_error("Wrong suite name in $filename line $.")
        if defined $1 and defined $suitename and $1 ne $suitename;
      $disabled{($1 || $suitename || '') . ".$2"} = $3;
    }
    close DISABLED;
  }
}

#
# load suite.pm files from plugin suites
# collect the list of default plugin suites.
# XXX currently it does not support nested suites
#
sub collect_default_suites(@)
{
  my @dirs = my_find_dir(dirname($::glob_mysql_test_dir),
                         [ @plugin_suitedirs ], '*');
  for my $d (@dirs) {
    next unless -f "$d/suite.pm";
    my $sname= basename($d);
    # ignore overlays here, otherwise we'd need accurate
    # duplicate detection with overlay support for the default suite list
    next if $sname eq 'main' or -d "$::glob_mysql_test_dir/suite/$sname";
    my $s = load_suite_object($sname, $d);
    push @_, $sname if $s->is_default();
  }
  return @_;
}


#
# processes one user-specified suite name.
# it could contain wildcards, e.g engines/*
#
sub collect_suite_name($$)
{
  my $suitename= shift;  # Test suite name
  my $opt_cases= shift;
  my $over;
  my %suites;

  ($suitename, $over) = split '-', $suitename;

  if ( $suitename ne "main" )
  {
    # Allow suite to be path to "some dir" if $suitename has at least
    # one directory part
    if ( -d $suitename and splitdir($suitename) > 1 ) {
      $suites{$suitename} = [ $suitename ];
      mtr_report(" - from '$suitename'");
    }
    else
    {
      my @dirs = my_find_dir(dirname($::glob_mysql_test_dir),
                             ["mysql-test/suite", @plugin_suitedirs ],
                             $suitename);
      #
      # if $suitename contained wildcards, we'll have many suites and
      # their overlays here. Let's group them appropriately.
      #
      for (@dirs) {
        m@^.*/(?:mysql-test/suite|$plugin_suitedir_regex)/(.*)$@o or confess $_;
        push @{$suites{$1}}, $_;
      }
    }
  } else {
    $suites{$suitename} = [ $::glob_mysql_test_dir,
                            my_find_dir(dirname($::glob_mysql_test_dir),
                                        [ @plugin_suitedirs ],
                                        'main', NOT_REQUIRED) ];
  }

  my @cases;
  while (my ($name, $dirs) = each %suites) {
    #
    # XXX at the moment, for simplicity, we will not fully support one
    # plugin overlaying a suite of another plugin. Only suites in the main
    # mysql-test directory can be safely overlayed. To be fixed, when
    # needed.  To fix it we'll need a smarter overlay detection (that is,
    # detection of what is an overlay and what is the "original" suite)
    # than simply "prefer directories with more files".
    #
    if ($dirs->[0] !~ m@/mysql-test/suite/$name$@) {
      # prefer directories with more files
      @$dirs = sort { scalar(<$a/*>) <=> scalar(<$b/*>) } @$dirs;
    }
    push @cases, collect_one_suite($opt_cases, $name, $over, @$dirs);
  }
  return @cases;
}

sub collect_one_suite {
  my ($opt_cases, $suitename, $over, $suitedir, @overlays) = @_;

  mtr_verbose("Collecting: $suitename");
  mtr_verbose("suitedir: $suitedir");
  mtr_verbose("overlays: @overlays") if @overlays;

  # we always need to process the parent suite, even if we won't use any
  # test from it.
  my @cases= process_suite($suitename, undef, $suitedir,
                           $over ? [ '*BOGUS*' ] : $opt_cases);

  # when working with overlays we cannot use global caches like
  # %file_to_tags. Because the same file may have different tags
  # with and without overlays. For example, when a.test includes
  # b.inc, which includes c.inc, and an overlay replaces c.inc.
  # In this case b.inc may have different tags in the overlay,
  # despite the fact that b.inc itself is not replaced.
  for (@overlays) {
    local %file_to_tags = ();
    local %file_to_master_opts = ();
    local %file_to_slave_opts = ();
    local %file_combinations = ();
    local %file_in_overlay = ();

    confess $_ unless m@/$overlay_regex/@o;
    next unless defined $over and ($over eq '' or $over eq $1);
    push @cases, 
    # don't add cases that take *all* data from the parent suite
      grep { $_->{in_overlay} } process_suite($suitename, $1, $_, $opt_cases);
  }
  return @cases;
}

sub process_suite {
  my ($basename, $overname, $suitedir, $opt_cases) = @_;
  my $suitename;
  my $parent;

  if ($overname) {
    $parent = $suites{$basename};
    confess unless $parent;
    $suitename = $basename . '-' . $overname;
  } else {
    $suitename = $basename;
  }

  my $suite = load_suite_object($suitename, $suitedir);

  #
  # Read suite config files, unless it was done aleady
  #
  unless (defined $suite->{name}) {
    $suite->{name} = $suitename;
    $suite->{dir}  = $suitedir;

    # First, we need to find where the test files and result files are.
    # test files are usually in a t/ dir inside suite dir. Or directly in the
    # suite dir. result files are in a r/ dir or in the suite dir.
    # Overlay uses t/ and r/ if and only if its parent does.
    if ($parent) {
      $suite->{parent} = $parent;
      my $tdir = $parent->{tdir};
      my $rdir = $parent->{rdir};
      substr($tdir, 0, length $parent->{dir}) = $suitedir;
      substr($rdir, 0, length $parent->{dir}) = $suitedir;
      $suite->{tdir} = $tdir if -d $tdir;
      $suite->{rdir} = $rdir if -d $rdir;
    } else {
      my $tdir= "$suitedir/t";
      my $rdir= "$suitedir/r";
      $suite->{tdir} = -d $tdir ? $tdir : $suitedir;
      $suite->{rdir} = -d $rdir ? $rdir : $suite->{tdir};
    }

    mtr_verbose("testdir: " . $suite->{tdir});
    mtr_verbose( "resdir: " . $suite->{rdir});

    # disabled.def
    parse_disabled($suite->{dir} .'/disabled.def', $suitename);

    # combinations
    if (@::opt_combinations)
    {
      # take the combination from command-line
      mtr_verbose("Take the combination from command line");
      foreach my $combination (@::opt_combinations) {
	my $comb= {};
	$comb->{name}= $combination;
	push(@{$comb->{comb_opt}}, $combination);
        push @{$suite->{combinations}}, $comb;
      }
    }
    else
    {
      my @combs;
      my $from =  "$suitedir/combinations";
      @combs = combinations_from_file($parent, $from) unless $suite->{skip};
      $suite->{combinations} = [ @combs ];
      #  in overlays it's a union of parent's and overlay's files.
      unshift @{$suite->{combinations}},
        grep { not $skip_combinations{"$from => $_->{name}"} }
          @{$parent->{combinations}} if $parent;
    }

    # suite.opt
    #  in overlays it's a union of parent's and overlay's files.
    $suite->{opts} = [ opts_from_file("$suitedir/suite.opt") ];
    $suite->{in_overlay} = 1 if $parent and @{$suite->{opts}};
    unshift @{$suite->{opts}}, @{$parent->{opts}} if $parent;

    $suite->{cases} = [ $suite->list_cases($suite->{tdir}) ];
  }

  my %all_cases;
  %all_cases = map { $_ => $parent->{tdir} } @{$parent->{cases}} if $parent;
  $all_cases{$_} = $suite->{tdir} for @{$suite->{cases}};

  my @cases;
  if (@$opt_cases) {
    # Collect in specified order
    foreach my $test_name_spec ( @$opt_cases )
    {
      my ($sname, $tname, @combs)= split_testname($test_name_spec);

      # Check correct suite if suitename is defined
      next if defined $sname and $sname ne $suitename
                             and $sname ne "$basename-";

      next unless $all_cases{$tname};
      push @cases, collect_one_test_case($suite, $all_cases{$tname}, $tname, @combs);
    }
  } else {
    for (sort keys %all_cases)
    {
      # Skip tests that do not match the --do-test= filter
      next if $do_test_reg and not /$do_test_reg/o;
      push @cases, collect_one_test_case($suite, $all_cases{$_}, $_);
    }
  }

  @cases;
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

sub make_combinations($$@)
{
  my ($test, $test_combs, @combinations) = @_;

  return ($test) if $test->{'skip'} or not @combinations;
  if ($combinations[0]->{skip}) {
    $test->{skip} = 1;
    $test->{comment} = $combinations[0]->{skip} unless $test->{comment};
    confess unless @combinations == 1;
    return ($test);
  }

  foreach my $comb (@combinations)
  {
    # Skip all other combinations if the values they change
    # are already fixed in master_opt or slave_opt
    if (My::Options::is_set($test->{master_opt}, $comb->{comb_opt}) &&
        My::Options::is_set($test->{slave_opt}, $comb->{comb_opt}) ){

      delete $test_combs->{$comb->{name}};

      # Add combination name short name
      push @{$test->{combinations}}, $comb->{name};

      return ($test);
    }

    # Skip all other combinations, if this combination is forced
    if (delete $test_combs->{$comb->{name}}) {
      @combinations = ($comb); # run the loop below only for this combination
      last;
    }
  }

  my @cases;
  foreach my $comb (@combinations)
  {
    # Copy test options
    my $new_test= $test->copy();
    
    # Prepend the combination options to master_opt and slave_opt
    # (on the command line combinations go *before* .opt files)
    unshift @{$new_test->{master_opt}}, @{$comb->{comb_opt}};
    unshift @{$new_test->{slave_opt}}, @{$comb->{comb_opt}};

    # Add combination name short name
    push @{$new_test->{combinations}}, $comb->{name};

    $new_test->{in_overlay} = 1 if $comb->{in_overlay};

    # Add the new test to new test cases list
    push(@cases, $new_test);
  }
  return @cases;
}


sub find_file_in_dirs
{
  my ($tinfo, $slot, $filename) = @_;
  my $parent = $tinfo->{suite}->{parent};
  my $f = $tinfo->{suite}->{$slot} . '/' . $filename;

  if (-f $f) {
    $tinfo->{in_overlay} = 1 if $parent;
    return $f;
  }

  return undef unless $parent;

  $f = $parent->{$slot} . '/' . $filename;
  return -f $f ? $f : undef;
}

##############################################################################
#
#  Collect information about a single test case
#
##############################################################################

sub collect_one_test_case {
  my $suite     =  shift;
  my $tpath     =  shift;
  my $tname     =  shift;
  my %test_combs = map { $_ => 1 } @_;


  my $suitename =  $suite->{name};
  my $name      = "$suitename.$tname";
  my $filename  = "$tpath/${tname}.test";

  # ----------------------------------------------------------------------
  # Set defaults
  # ----------------------------------------------------------------------
  my $tinfo= My::Test->new
    (
     name          => $name,
     shortname     => $tname,
     path          => $filename,
     suite         => $suite,
     in_overlay    => $suite->{in_overlay},
     master_opt    => [ @{$suite->{opts}} ],
     slave_opt     => [ @{$suite->{opts}} ],
    );

  # ----------------------------------------------------------------------
  # Skip some tests but include in list, just mark them as skipped
  # ----------------------------------------------------------------------
  if ( $skip_test_reg and ($tname =~ /$skip_test_reg/o or
                            $name =~ /$skip_test_reg/o))
  {
    $tinfo->{'skip'}= 1;
    return $tinfo;
  }

  # ----------------------------------------------------------------------
  # Check for disabled tests
  # ----------------------------------------------------------------------
  my $disable = $disabled{".$tname"} || $disabled{$name};
  if (not defined $disable and $suite->{parent}) {
    $disable = $disabled{$suite->{parent}->{name} . ".$tname"};
  }
  if (defined $disable)
  {
    $tinfo->{comment}= $disable;
    if ( $enable_disabled )
    {
      # User has selected to run all disabled tests
      mtr_report(" - $tinfo->{name} wil be run although it's been disabled\n",
		 "  due to '$disable'");
    }
    else
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'disable'}= 1;   # Sub type of 'skip'

      # we can stop test file processing early if the test if disabled, but
      # only if we're not in the overlay.  for overlays we want to know exactly
      # whether the test is ignored (in_overlay=0) or disabled.
      return $tinfo unless $suite->{parent};
    }
  }

  if ($suite->{skip}) {
    $tinfo->{skip}= 1;
    $tinfo->{comment}= $suite->{skip} unless $tinfo->{comment};
    return $tinfo unless $suite->{parent};
  }

  # ----------------------------------------------------------------------
  # Check for test specific config file
  # ----------------------------------------------------------------------
  my $test_cnf_file= find_file_in_dirs($tinfo, tdir => "$tname.cnf");
  if ($test_cnf_file ) {
    # Specifies the configuration file to use for this test
    $tinfo->{'template_path'}= $test_cnf_file;
  }

  # ----------------------------------------------------------------------
  # master sh
  # ----------------------------------------------------------------------
  my $master_sh= find_file_in_dirs($tinfo, tdir => "$tname-master.sh");
  if ($master_sh)
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
  my $slave_sh= find_file_in_dirs($tinfo, tdir => "$tname-slave.sh");
  if ($slave_sh)
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

  my ($master_opts, $slave_opts)= tags_from_test_file($tinfo);
  $tinfo->{in_overlay} = 1 if $file_in_overlay{$filename};

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

  if ( $tinfo->{'rpl_test'} )
  {
    if ( $skip_rpl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No replication tests";
      return $tinfo;
    }
  }

  # ----------------------------------------------------------------------
  # Find config file to use if not already selected in <testname>.opt file
  # ----------------------------------------------------------------------
  if (not $tinfo->{template_path} )
  {
    my $config= find_file_in_dirs($tinfo, dir => 'my.cnf');
    if (not $config)
    {
      # Suite has no config, autodetect which one to use
      if ($tinfo->{rpl_test}) {
        $config= "suite/rpl/my.cnf";
      } else {
        $config= "include/default_my.cnf";
      }
    }
    $tinfo->{template_path}= $config;
  }

  # ----------------------------------------------------------------------
  # Append mysqld extra options to master and slave, as appropriate
  # ----------------------------------------------------------------------
  push @{$tinfo->{'master_opt'}}, @$master_opts, @::opt_extra_mysqld_opt;
  push @{$tinfo->{'slave_opt'}}, @$slave_opts, @::opt_extra_mysqld_opt;

  process_opts($tinfo, 'master_opt');
  process_opts($tinfo, 'slave_opt');

  my @cases = ($tinfo);
  for my $comb ($suite->{combinations}, @{$file_combinations{$filename}})
  {
    @cases = map make_combinations($_, \%test_combs, @{$comb}), @cases;
  }
  if (keys %test_combs) {
    mtr_error("Could not run $name with '".(
        join(',', sort keys %test_combs))."' combination(s)");
  }

  for $tinfo (@cases) {
    #
    # Now we find a result file for every test file. It's a bit complicated.
    # For a test foobar.test in the combination pair {aa,bb}, and in the
    # overlay "rty" to the suite "qwe", in other words, for the
    # that that mtr prints as
    #   ...
    #   qwe-rty.foobar                   'aa,bb'  [ pass ]
    #   ...
    # the result can be expected in
    #  * either .rdiff or .result file
    #  * either in the overlay or in the original suite
    #  * with or without combinations in the file name.
    # which means any of the following 15 file names can be used:
    #
    #  1    rty/r/foo,aa,bb.result          
    #  2    rty/r/foo,aa,bb.rdiff
    #  3    qwe/r/foo,aa,bb.result
    #  4    qwe/r/foo,aa,bb.rdiff
    #  5    rty/r/foo,aa.result
    #  6    rty/r/foo,aa.rdiff
    #  7    qwe/r/foo,aa.result
    #  8    qwe/r/foo,aa.rdiff
    #  9    rty/r/foo,bb.result
    # 10    rty/r/foo,bb.rdiff
    # 11    qwe/r/foo,bb.result
    # 12    qwe/r/foo,bb.rdiff
    # 13    rty/r/foo.result
    # 14    rty/r/foo.rdiff
    # 15    qwe/r/foo.result
    #
    # They are listed, precisely, in the order of preference.
    # mtr will walk that list from top to bottom and the first file that
    # is found will be used.
    #
    # If this found file is a .rdiff, mtr continues walking down the list
    # until the first .result file is found.
    # A .rdiff is applied to that .result.
    #
    my $re ='';

    if ($tinfo->{combinations}) {
      $re = '(?:' . join('|', @{$tinfo->{combinations}}) . ')';
    }
    my $resdirglob = $suite->{rdir};
    $resdirglob.= ',' . $suite->{parent}->{rdir} if $suite->{parent};

    my %files;
    for (<{$resdirglob}/$tname*.{rdiff,result}>) {
      my ($path, $combs, $ext) =
                  m@^(.*)/$tname((?:,$re)*)\.(rdiff|result)$@ or next;
      my @combs = sort split /,/, $combs;
      $files{$_} = join '~', (                # sort files by
        99 - scalar(@combs),                  # number of combinations DESC
        join(',', sort @combs),               # combination names ASC
        $path eq $suite->{rdir} ? 1 : 2,      # overlay first
        $ext eq 'result' ? 1 : 2              # result before rdiff
      );
    }
    my @results = sort { $files{$a} cmp $files{$b} } keys %files;

    if (@results) {
      my $result_file = shift @results;
      $tinfo->{result_file} = $result_file;

      if ($result_file =~ /\.rdiff$/) {
        shift @results while $results[0] =~ /\.rdiff$/;
        mtr_error ("$result_file has no corresponding .result file")
          unless @results;
        $tinfo->{base_result} = $results[0];

        if (not $::exe_patch) {
          $tinfo->{skip} = 1;
          $tinfo->{comment} = "requires patch executable";
        }
      }
    } else {
      # No .result file exist
      # Remember the path  where it should be
      # saved in case of --record
      $tinfo->{record_file}= $suite->{rdir} . "/$tname.result";
    }
  }

  return @cases;
}


my $tags_map= {'big_test' => ['big_test', 1],
               'have_ndb' => ['ndb_test', 1],
               'have_multi_ndb' => ['ndb_test', 1],
               'master-slave' => ['rpl_test', 1],
               'ndb_master-slave' => ['rpl_test', 1, 'ndb_test', 1],
               'long_test' => ['long_test', 1],
};
my $tags_regex_string= join('|', keys %$tags_map);
my $tags_regex= qr:include/($tags_regex_string)\.inc:o;

# Get various tags from a file, recursively scanning also included files.
# And get options from .opt file, also recursively for included files.
# Return a list of [TAG_TO_SET, VALUE_TO_SET_TO] of found tags.
# Also returns lists of options for master and slave found in .opt files.
# Each include file is scanned only once, and subsequent calls just look up the
# cached result.
# We need to be a bit careful about speed here; previous version of this code
# took forever to scan the full test suite.
sub get_tags_from_file($$) {
  my ($file, $suite)= @_;

  return @{$file_to_tags{$file}} if exists $file_to_tags{$file};

  my $F= IO::File->new($file)
    or mtr_error("can't open file \"$file\": $!");

  my $tags= [];
  my $master_opts= [];
  my $slave_opts= [];
  my @combinations;

  my $over = defined $suite->{parent};
  my $sdir = $suite->{dir};
  my $pdir = $suite->{parent}->{dir} if $over;
  my $in_overlay = 0;
  my $suffix = $file;
  my @prefix = ('');

  # to be able to look up all auxillary files in the overlay
  # we split the file path in a prefix and a suffix
  if ($file =~ m@^$sdir/(.*)$@) {
    $suffix = $1;
    @prefix =  ("$sdir/");
    push @prefix, "$pdir/" if $over;
    $in_overlay = $over;
  } elsif ($over and $file =~ m@^$pdir/(.*)$@) {
    $suffix = $1;
    @prefix = map { "$_/" } $sdir, $pdir;
  } else {
    $over = 0; # file neither in $sdir nor in $pdir
  }

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
      # The rules below must match open_file() function of mysqltest.cc
      # Note that for the purpose of tag collection we ignore
      # non-existing files, and let mysqltest handle the error
      # (e.g. mysqltest.test needs this)
      for ((map { dirname("$_$suffix") } @prefix),
           $sdir, $pdir, $::glob_mysql_test_dir)
      {
        next unless defined $_;
        my $sourced_file = "$_/$include";
        next if $sourced_file eq $file;
        if (-e $sourced_file)
        {
          push @$tags, get_tags_from_file($sourced_file, $suite);
          push @$master_opts, @{$file_to_master_opts{$sourced_file}};
          push @$slave_opts, @{$file_to_slave_opts{$sourced_file}};
          push @combinations, @{$file_combinations{$sourced_file}};
          $file_in_overlay{$file} ||= $file_in_overlay{$sourced_file};
          last;
        }
      }
    }
  }

  # Add options from main file _after_ those of any includes; this allows a
  # test file to override options set by includes (eg. rpl.rpl_ddl uses this
  # to enable innodb, then disable innodb in the slave.
  $suffix =~ s/\.\w+$//;

  for (qw(.opt -master.opt -slave.opt)) {
    my @res;
    push @res, opts_from_file("$prefix[1]$suffix$_") if $over;
    if (-f "$prefix[0]$suffix$_") {
      $in_overlay = $over;
      push @res, opts_from_file("$prefix[0]$suffix$_");
    }
    push @$master_opts, @res unless /slave/;
    push @$slave_opts, @res unless /master/;
  }

  # for combinations we need to make sure that its suite object is loaded,
  # even if this file does not belong to a current suite!
  my $comb_file = "$suffix.combinations";
  $suite = load_suite_object(suite_for_file($comb_file)) if $prefix[0] eq '';
  my @comb;
  unless ($suite->{skip}) {
    my $from = "$prefix[0]$comb_file";
    @comb = combinations_from_file($over, $from);
    push @comb,
      grep { not $skip_combinations{"$from => $_->{name}"} }
        combinations_from_file(undef, "$prefix[1]$comb_file") if $over;
  }
  push @combinations, [ @comb ];

  # Save results so we can reuse without parsing if seen again.
  $file_to_tags{$file}= $tags;
  $file_to_master_opts{$file}= $master_opts;
  $file_to_slave_opts{$file}= $slave_opts;
  $file_combinations{$file}= [ uniq(@combinations) ];
  $file_in_overlay{$file} = 1 if $in_overlay;
  return @{$tags};
}

sub tags_from_test_file {
  my ($tinfo)= @_;
  my $file = $tinfo->{path};

  # a suite may generate tests that don't map to real *.test files
  # see unit suite for an example.
  return ([], []) unless -f $file;

  for (get_tags_from_file($file, $tinfo->{suite}))
  {
    $tinfo->{$_->[0]}= $_->[1];
  }
  return ($file_to_master_opts{$file}, $file_to_slave_opts{$file});
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

  open(FILE, '<', $file) or mtr_error("can't open file \"$file\": $!");
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

