# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use File::Basename;
use IO::File();
use strict;

sub collect_test_cases ($);
sub collect_one_test_case ($$$$$$$);

sub mtr_options_from_test_file($$);

##############################################################################
#
#  Collect information about test cases we are to run
#
##############################################################################

sub collect_test_cases ($) {
  my $suite= shift;             # Test suite name

  my $testdir;
  my $resdir;

  if ( $suite eq "main" )
  {
    $testdir= "$::glob_mysql_test_dir/t";
    $resdir=  "$::glob_mysql_test_dir/r";
  }
  else
  {
    $testdir= "$::glob_mysql_test_dir/suite/$suite/t";
    $resdir=  "$::glob_mysql_test_dir/suite/$suite/r";
  }

  my $cases = [];           # Array of hash, will be array of C struct

  opendir(TESTDIR, $testdir) or mtr_error("Can't open dir \"$testdir\": $!");

  # ----------------------------------------------------------------------
  # Disable some tests listed in disabled.def
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

  if ( @::opt_cases )
  {
    foreach my $tname ( @::opt_cases ) { # Run in specified order, no sort
      my $elem= undef;
      my $component_id= undef;
      
      # Get rid of directory part (path). Leave the extension since it is used
      # to understand type of the test.

      $tname = basename($tname);

      # Check if the extenstion has been specified.

      if ( mtr_match_extension($tname, "test") )
      {
        $elem= $tname;
        $tname=~ s/\.test$//;
        $component_id= 'mysqld';
      }
      elsif ( mtr_match_extension($tname, "imtest") )
      {
        $elem= $tname;
        $tname =~ s/\.imtest$//;
        $component_id= 'im';
      }

      # If target component is known, check that the specified test case
      # exists.
      # 
      # Otherwise, try to guess the target component.

      if ( $component_id )
      {
        if ( ! -f "$testdir/$elem")
        {
          mtr_error("Test case $tname ($testdir/$elem) is not found");
        }
      }
      else
      {
        my $mysqld_test_exists = -f "$testdir/$tname.test";
        my $im_test_exists = -f "$testdir/$tname.imtest";

        if ( $mysqld_test_exists and $im_test_exists )
        {
          mtr_error("Ambiguous test case name ($tname)");
        }
        elsif ( ! $mysqld_test_exists and ! $im_test_exists )
        {
          mtr_error("Test case $tname is not found");
        }
        elsif ( $mysqld_test_exists )
        {
          $elem= "$tname.test";
          $component_id= 'mysqld';
        }
        elsif ( $im_test_exists )
        {
          $elem= "$tname.imtest";
          $component_id= 'im';
        }
      }

      collect_one_test_case($testdir,$resdir,$tname,$elem,$cases,\%disabled,
        $component_id);
    }
    closedir TESTDIR;
  }
  else
  {
    foreach my $elem ( sort readdir(TESTDIR) ) {
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
      
      next if $::opt_do_test and ! defined mtr_match_prefix($elem,$::opt_do_test);

      collect_one_test_case($testdir,$resdir,$tname,$elem,$cases,\%disabled,
        $component_id);
    }
    closedir TESTDIR;
  }

  # To speed things up, we sort first in if the test require a restart
  # or not, second in alphanumeric order.

  if ( $::opt_reorder )
  {

    my %sort_criteria;
    my $tinfo;

    # Make a mapping of test name to a string that represents how that test
    # should be sorted among the other tests.  Put the most important criterion
    # first, then a sub-criterion, then sub-sub-criterion, et c.
    foreach $tinfo (@$cases) 
    {
      my @this_criteria = ();

      # Append the criteria for sorting, in order of importance.
      push(@this_criteria, join("!", sort @{$tinfo->{'master_opt'}}) . "~");  # Ending with "~" makes empty sort later than filled
      push(@this_criteria, "ndb=" . ($tinfo->{'ndb_test'} ? "1" : "0"));
      push(@this_criteria, "restart=" . ($tinfo->{'master_restart'} ? "1" : "0"));
      push(@this_criteria, "big_test=" . ($tinfo->{'big_test'} ? "1" : "0"));
      push(@this_criteria, join("|", sort keys %{$tinfo}));  # Group similar things together.  The values may differ substantially.  FIXME?
      push(@this_criteria, $tinfo->{'name'});   # Finally, order by the name
      
      $sort_criteria{$tinfo->{"name"}} = join(" ", @this_criteria);
    }

    @$cases = sort { $sort_criteria{$a->{"name"}} cmp $sort_criteria{$b->{"name"}}; } @$cases;

###  For debugging the sort-order
#    foreach $tinfo (@$cases) 
#    {
#      print $sort_criteria{$tinfo->{"name"}};
#      print " -> \t";
#      print $tinfo->{"name"};
#      print "\n";
#    }
  }

  return $cases;
}


##############################################################################
#
#  Collect information about a single test case
#
##############################################################################


sub collect_one_test_case($$$$$$$) {
  my $testdir= shift;
  my $resdir=  shift;
  my $tname=   shift;
  my $elem=    shift;
  my $cases=   shift;
  my $disabled=shift;
  my $component_id= shift;

  my $path= "$testdir/$elem";

  # ----------------------------------------------------------------------
  # Skip some tests silently
  # ----------------------------------------------------------------------

  if ( $::opt_start_from and $tname lt $::opt_start_from )
  {
    return;
  }

  # ----------------------------------------------------------------------
  # Skip some tests but include in list, just mark them to skip
  # ----------------------------------------------------------------------

  my $tinfo= {};
  $tinfo->{'name'}= $tname;
  $tinfo->{'result_file'}= "$resdir/$tname.result";
  $tinfo->{'component_id'} = $component_id;
  push(@$cases, $tinfo);

  if ( $::opt_skip_test and defined mtr_match_prefix($tname,$::opt_skip_test) )
  {
    $tinfo->{'skip'}= 1;
    return;
  }

  # ----------------------------------------------------------------------
  # Collect information about test case
  # ----------------------------------------------------------------------

  $tinfo->{'path'}= $path;
  $tinfo->{'timezone'}= "GMT-3"; # for UNIX_TIMESTAMP tests to work

  if ( defined mtr_match_prefix($tname,"rpl") )
  {
    if ( $::opt_skip_rpl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No replication tests(--skip-rpl)";
      return;
    }

    $tinfo->{'slave_num'}= 1;           # Default, use one slave

    if ( $tname eq 'rpl_failsafe' or $tname eq 'rpl_chain_temp_table' )
    {
      # $tinfo->{'slave_num'}= 3;         # Not 3 ? Check old code, strange
    }
  }

  if ( defined mtr_match_prefix($tname,"federated") )
  {
    # Default, federated uses the first slave as it's federated database
    $tinfo->{'slave_num'}= 1;
  }

  if ( $::opt_with_ndbcluster or defined mtr_match_substring($tname,"ndb") )
  {
    # This is an ndb test or all tests should be run with ndb cluster started
    $tinfo->{'ndb_test'}= 1;
    if ( $::opt_skip_ndbcluster )
    {
      # All ndb test's should be skipped
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No ndbcluster test(--skip-ndbcluster)";
      return;
    }
    if ( ! $::opt_ndbcluster_supported )
    {
      # Ndb is not supported, skip them
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No ndbcluster support";
      return;
    }
  }
  else
  {
    # This is not a ndb test
    $tinfo->{'ndb_test'}= 0;
    if ( $::opt_with_ndbcluster_only )
    {
      # Only the ndb test should be run, all other should be skipped
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Only ndbcluster tests(--with-ndbcluster-only)";
      return;
    }
  }

  # FIXME what about embedded_server + ndbcluster, skip ?!

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

  if ( -f $master_opt_file )
  {
    $tinfo->{'master_restart'}= 1;    # We think so for now

  MASTER_OPT:
    {
      my $master_opt= mtr_get_opts_from_file($master_opt_file);

      foreach my $opt ( @$master_opt )
      {
        my $value;

        # This is a dirty hack from old mysql-test-run, we use the opt
        # file to flag other things as well, it is not a opt list at
        # all

        $value= mtr_match_prefix($opt, "--timezone=");
        if ( defined $value )
        {
          $tinfo->{'timezone'}= $value;
          last MASTER_OPT;
        }

        $value= mtr_match_prefix($opt, "--result-file=");
        if ( defined $value )
        {
          $tinfo->{'result_file'}= "r/$value.result";
          if ( $::opt_result_ext and $::opt_record or
               -f "$tinfo->{'result_file'}$::opt_result_ext")
          {
            $tinfo->{'result_file'}.= $::opt_result_ext;
          }
          $tinfo->{'master_restart'}= 0;
          last MASTER_OPT;
        }

        # If we set default time zone, remove the one we have
        $value= mtr_match_prefix($opt, "--default-time-zone=");
        if ( defined $value )
        {
          $tinfo->{'master_opt'}= [];
        }

      }

      # Ok, this was a real option list, add it
      push(@{$tinfo->{'master_opt'}}, @$master_opt);
    }
  }

  if ( -f $slave_opt_file )
  {
    $tinfo->{'slave_restart'}= 1;
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
    $tinfo->{'slave_restart'}= 1;
  }

  if ( -f $master_sh )
  {
    if ( $::glob_win32_perl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No tests with sh scripts on Windows";
    }
    else
    {
      $tinfo->{'master_sh'}= $master_sh;
      $tinfo->{'master_restart'}= 1;
    }
  }

  if ( -f $slave_sh )
  {
    if ( $::glob_win32_perl )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No tests with sh scripts on Windows";
    }
    else
    {
      $tinfo->{'slave_sh'}= $slave_sh;
      $tinfo->{'slave_restart'}= 1;
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
    }
  }

  if ( $component_id eq 'im' )
  {
    if ( $::glob_use_embedded_server )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No IM with embedded server";
    }
    elsif ( $::opt_ps_protocol )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No IM with --ps-protocol";
    }
    elsif ( $::opt_skip_im )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "No IM tests(--skip-im)";
    }
  }
  else
  {
    mtr_options_from_test_file($tinfo,"$testdir/${tname}.test");

    if ( $tinfo->{'big_test'} and ! $::opt_big_test )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test need 'big-test' option";
    }

    if ( $tinfo->{'ndb_extra'} and ! $::opt_ndb_extra_test )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test need 'ndb_extra' option";
    }

    if ( $tinfo->{'require_manager'} )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test need the _old_ manager(to be removed)";
    }

    if ( defined $tinfo->{'binlog_format'} and
	 ! ( $tinfo->{'binlog_format'} eq $::used_binlog_format ) )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Not running with binlog format '$tinfo->{'binlog_format'}'";
    }

    if ( $tinfo->{'need_debug'} && ! $::debug_compiled_binaries )
    {
      $tinfo->{'skip'}= 1;
      $tinfo->{'comment'}= "Test need debug binaries";
    }
  }

  # We can't restart a running server that may be in use

  if ( $::glob_use_running_server and
       ( $tinfo->{'master_restart'} or $tinfo->{'slave_restart'} ) )
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'comment'}= "Can't restart a running server";
  }

}


# List of tags in the .test files that if found should set
# the specified value in "tinfo"
our @tags=
(
 ["include/have_innodb.inc", "innodb_test", 1],
 ["include/have_binlog_format_row.inc", "binlog_format", "row"],
 ["include/have_binlog_format_statement.inc", "binlog_format", "stmt"],
 ["include/big_test.inc", "big_test", 1],
 ["include/have_debug.inc", "need_debug", 1],
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
    chomp;

    next if ( $line !~ /^--/ );

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
    if ( $line =~ /^\-\-([[:space:]]*)source(.*)$/ )
    {
      my $value= $2;
      $value =~ s/^\s+//;  # Remove leading space
      $value =~ s/[[:space:]]+$//;  # Remove ending space

      my $sourced_file= "$::glob_mysql_test_dir/$value";
      mtr_options_from_test_file($tinfo, $sourced_file);
    }

  }
}

1;
