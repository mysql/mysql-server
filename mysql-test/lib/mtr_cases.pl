# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub collect_test_cases ($);
sub collect_one_test_case ($$$$$);

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

  if ( @::opt_cases )
  {
    foreach my $tname ( @::opt_cases ) { # Run in specified order, no sort
      my $elem= "$tname.test";
      if ( ! -f "$testdir/$elem")
      {
        mtr_error("Test case $tname ($testdir/$elem) is not found");
      }
      collect_one_test_case($testdir,$resdir,$tname,$elem,$cases);
    }
    closedir TESTDIR;
  }
  else
  {
    foreach my $elem ( sort readdir(TESTDIR) ) {
      my $tname= mtr_match_extension($elem,"test");
      next if ! defined $tname;
      next if $::opt_do_test and ! defined mtr_match_prefix($elem,$::opt_do_test);

      collect_one_test_case($testdir,$resdir,$tname,$elem,$cases);
    }
    closedir TESTDIR;
  }

  # To speed things up, we sort first in if the test require a restart
  # or not, second in alphanumeric order.

  if ( $::opt_reorder )
  {
    @$cases = sort {
      if ( $a->{'master_restart'} and $b->{'master_restart'} or
           ! $a->{'master_restart'} and ! $b->{'master_restart'} )
      {
        return $a->{'name'} cmp $b->{'name'};
      }
      if ( $a->{'master_restart'} )
      {
        return 1;                 # Is greater
      }
      else
      {
        return -1;                # Is less
      }
    } @$cases;
  }

  return $cases;
}


##############################################################################
#
#  Collect information about a single test case
#
##############################################################################


sub collect_one_test_case($$$$$) {
  my $testdir= shift;
  my $resdir=  shift;
  my $tname=   shift;
  my $elem=    shift;
  my $cases=   shift;

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
  push(@$cases, $tinfo);

  if ( $::opt_skip_test and defined mtr_match_prefix($tname,$::opt_skip_test) )
  {
    $tinfo->{'skip'}= 1;
    return;
  }

  # FIXME temporary solution, we have a hard coded list of test cases to
  # skip if we are using the embedded server

  if ( $::glob_use_embedded_server and
       mtr_match_any_exact($tname,\@::skip_if_embedded_server) )
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
      return;
    }

    $tinfo->{'slave_num'}= 1;           # Default, use one slave

    # FIXME currently we always restart slaves
    $tinfo->{'slave_restart'}= 1;

    if ( $tname eq 'rpl_failsafe' or $tname eq 'rpl_chain_temp_table' )
    {
#      $tinfo->{'slave_num'}= 3;         # Not 3 ? Check old code, strange
    }
  }

  # FIXME what about embedded_server + ndbcluster, skip ?!

  my $master_opt_file= "$testdir/$tname-master.opt";
  my $slave_opt_file=  "$testdir/$tname-slave.opt";
  my $slave_mi_file=   "$testdir/$tname.slave-mi";
  my $master_sh=       "$testdir/$tname-master.sh";
  my $slave_sh=        "$testdir/$tname-slave.sh";
  my $disabled=        "$testdir/$tname.disabled";

  $tinfo->{'master_opt'}= [];
  $tinfo->{'slave_opt'}=  [];
  $tinfo->{'slave_mi'}=   [];

  if ( -f $master_opt_file )
  {
    $tinfo->{'master_restart'}= 1;    # We think so for now
    # This is a dirty hack from old mysql-test-run, we use the opt file
    # to flag other things as well, it is not a opt list at all
    my $extra_master_opt= mtr_get_opts_from_file($master_opt_file);

    foreach my $opt (@$extra_master_opt)
    {
      my $value;

      $value= mtr_match_prefix($opt, "--timezone=");

      if ( defined $value )
      {
        $tinfo->{'timezone'}= $value;
        $extra_master_opt= [];
        $tinfo->{'master_restart'}= 0;
        last;
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
        $extra_master_opt= [];
        $tinfo->{'master_restart'}= 0;
        last;
      }
    }

    $tinfo->{'master_opt'}= $extra_master_opt;
  }

  if ( -f $slave_opt_file )
  {
    $tinfo->{'slave_opt'}= mtr_get_opts_from_file($slave_opt_file);
    $tinfo->{'slave_restart'}= 1;
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
    }
    else
    {
      $tinfo->{'slave_sh'}= $slave_sh;
      $tinfo->{'slave_restart'}= 1;
    }
  }

  if ( -f $disabled )
  {
    $tinfo->{'skip'}= 1;
    $tinfo->{'disable'}= 1;   # Sub type of 'skip'
    $tinfo->{'comment'}= mtr_fromfile($disabled);
  }

  # We can't restart a running server that may be in use

  if ( $::glob_use_running_server and
       ( $tinfo->{'master_restart'} or $tinfo->{'slave_restart'} ) )
  {
    $tinfo->{'skip'}= 1;
  }
}


1;
