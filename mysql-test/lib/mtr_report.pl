# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub mtr_report_test_name($);
sub mtr_report_test_passed($);
sub mtr_report_test_failed($);
sub mtr_report_test_skipped($);
sub mtr_report_test_not_skipped_though_disabled($);

sub mtr_show_failed_diff ($);
sub mtr_report_stats ($);
sub mtr_print_line ();
sub mtr_print_thick_line ();
sub mtr_print_header ();
sub mtr_report (@);
sub mtr_warning (@);
sub mtr_error (@);
sub mtr_child_error (@);
sub mtr_debug (@);
sub mtr_verbose (@);


##############################################################################
#
#  
#
##############################################################################

# We can't use diff -u or diff -a as these are not portable

sub mtr_show_failed_diff ($) {
  my $tname=  shift;

  my $reject_file=  "r/$tname.reject";
  my $result_file=  "r/$tname.result";
  my $log_file=  "r/$tname.log";
  my $eval_file=    "r/$tname.eval";

  if ( $::opt_suite ne "main" )
  {
    $reject_file= "$::glob_mysql_test_dir/suite/$::opt_suite/$reject_file";
    $result_file= "$::glob_mysql_test_dir/suite/$::opt_suite/$result_file";
    $eval_file=   "$::glob_mysql_test_dir/suite/$::opt_suite/$eval_file";
    $log_file=   "$::glob_mysql_test_dir/suite/$::opt_suite/$log_file";
  }

  if ( -f $eval_file )
  {
    $result_file=  $eval_file;
  }
  elsif ( $::opt_result_ext and
          ( $::opt_record or -f "$result_file$::opt_result_ext" ))
  {
    # If we have an special externsion for result files we use it if we are
    # recording or a result file with that extension exists.
    $result_file=  "$result_file$::opt_result_ext";
  }

  my $diffopts= $::opt_udiff ? "-u" : "-c";

  if ( -f $reject_file )
  {
    print "Below are the diffs between actual and expected results:\n";
    print "-------------------------------------------------------\n";
    # FIXME check result code?!
    mtr_run("diff",[$diffopts,$result_file,$reject_file], "", "", "", "");
    print "-------------------------------------------------------\n";
    print "Please follow the instructions outlined at\n";
    print "http://www.mysql.com/doc/en/Reporting_mysqltest_bugs.html\n";
    print "to find the reason to this problem and how to report this.\n\n";
  }

  if ( -f $log_file )
  {
    print "Result from queries before failure can be found in $log_file\n";
    # FIXME Maybe a tail -f -n 10 $log_file here
  }
}

sub mtr_report_test_name ($) {
  my $tinfo= shift;

  printf "%-30s ", $tinfo->{'name'};
}

sub mtr_report_test_skipped ($) {
  my $tinfo= shift;

  $tinfo->{'result'}= 'MTR_RES_SKIPPED';
  if ( $tinfo->{'disable'} )
  {
    print "[ disabled ]  $tinfo->{'comment'}\n";
  }
  else
  {
    print "[ skipped ]   $tinfo->{'comment'}\n";
  }
}

sub mtr_report_tests_not_skipped_though_disabled ($) {
  my $tests= shift;

  if ( $::opt_enable_disabled )
  {
    my @disabled_tests= grep {$_->{'dont_skip_though_disabled'}} @$tests;
    if ( @disabled_tests )
    {
      print "\nTest(s) which will be run though they are marked as disabled:\n";
      foreach my $tinfo ( sort {$a->{'name'} cmp $b->{'name'}} @disabled_tests )
      {
        printf "  %-20s : %s\n", $tinfo->{'name'}, $tinfo->{'comment'};
      }
    }
  }
}

sub mtr_report_test_passed ($) {
  my $tinfo= shift;

  my $timer=  "";
  if ( $::opt_timer and -f "$::opt_vardir/log/timer" )
  {
    $timer= mtr_fromfile("$::opt_vardir/log/timer");
    $::glob_tot_real_time += ($timer/1000);
    $timer= sprintf "%12s", $timer;
  }
  $tinfo->{'result'}= 'MTR_RES_PASSED';
  print "[ pass ]   $timer\n";
}

sub mtr_report_test_failed ($) {
  my $tinfo= shift;

  $tinfo->{'result'}= 'MTR_RES_FAILED';
  if ( $tinfo->{'timeout'} )
  {
    print "[ fail ]  timeout\n";
  }
  elsif ( $tinfo->{'ndb_test'} and $::cluster->[0]->{'installed_ok'} eq "NO")
  {
    print "[ fail ]  ndbcluster start failure\n";
    return;
  }
  else
  {
    print "[ fail ]\n";
  }

  # FIXME Instead of this test, and meaningless error message in 'else'
  # we should write out into $::path_timefile when the error occurs.
  if ( -f $::path_timefile )
  {
    print "\nErrors are (from $::path_timefile) :\n";
    print mtr_fromfile($::path_timefile); # FIXME print_file() instead
    print "\n(the last lines may be the most important ones)\n";
  }
  else
  {
    print "\nUnexpected termination, probably when starting mysqld\n";
  }
}

sub mtr_report_stats ($) {
  my $tests= shift;

  # ----------------------------------------------------------------------
  # Find out how we where doing
  # ----------------------------------------------------------------------

  my $tot_skiped= 0;
  my $tot_passed= 0;
  my $tot_failed= 0;
  my $tot_tests=  0;
  my $tot_restarts= 0;
  my $found_problems= 0;            # Some warnings are errors...

  foreach my $tinfo (@$tests)
  {
    if ( $tinfo->{'result'} eq 'MTR_RES_SKIPPED' )
    {
      $tot_skiped++;
    }
    elsif ( $tinfo->{'result'} eq 'MTR_RES_PASSED' )
    {
      $tot_tests++;
      $tot_passed++;
    }
    elsif ( $tinfo->{'result'} eq 'MTR_RES_FAILED' )
    {
      $tot_tests++;
      $tot_failed++;
    }
    if ( $tinfo->{'restarted'} )
    {
      $tot_restarts++;
    }
  }

  # ----------------------------------------------------------------------
  # Print out a summary report to screen
  # ----------------------------------------------------------------------

  if ( ! $tot_failed )
  {
    print "All $tot_tests tests were successful.\n";
  }
  else
  {
    my $ratio=  $tot_passed * 100 / $tot_tests;
    printf "Failed $tot_failed/$tot_tests tests, " .
      "%.2f\% were successful.\n\n", $ratio;
    print
      "The log files in var/log may give you some hint\n",
      "of what went wrong.\n",
      "If you want to report this error, please read first ",
      "the documentation at\n",
      "http://www.mysql.com/doc/en/MySQL_test_suite.html\n";
  }
  print
      "The servers were restarted $tot_restarts times\n";

  if ( $::opt_timer )
  {
    print
      "Spent $::glob_tot_real_time seconds actually executing testcases\n"
  }

  # ----------------------------------------------------------------------
  # If a debug run, there might be interesting information inside
  # the "var/log/*.err" files. We save this info in "var/log/warnings"
  # ----------------------------------------------------------------------

  if ( ! $::glob_use_running_server )
  {
    # Save and report if there was any fatal warnings/errors in err logs

    my $warnlog= "$::opt_vardir/log/warnings";

    unless ( open(WARN, ">$warnlog") )
    {
      mtr_warning("can't write to the file \"$warnlog\": $!");
    }
    else
    {
      # We report different types of problems in order
      foreach my $pattern ( "^Warning:", "^Error:", "^==.* at 0x",
			    "InnoDB: Warning", "missing DBUG_RETURN",
			    "mysqld: Warning",
			    "Attempting backtrace", "Assertion .* failed" )
      {
        foreach my $errlog ( sort glob("$::opt_vardir/log/*.err") )
        {
          unless ( open(ERR, $errlog) )
          {
            mtr_warning("can't read $errlog");
            next;
          }
          while ( <ERR> )
          {
            # Skip some non fatal warnings from the log files
            if ( /Warning:\s+Table:.* on (delete|rename)/ or
                 /Warning:\s+Setting lower_case_table_names=2/ or
                 /Warning:\s+One can only use the --user.*root/ or
	         /InnoDB: Warning: we did not need to do crash recovery/)
            {
              next;                       # Skip these lines
            }
            if ( /$pattern/ )
            {
              $found_problems= 1;
              print WARN $_;
            }
          }
        }
      }
      if ( $found_problems )
      {
	mtr_warning("Got errors/warnings while running tests, please examine",
		    "\"$warnlog\" for details.");
      }
    }
  }

  print "\n";

  if ( $tot_failed != 0 )
  {
    my $test_mode= join(" ", @::glob_test_mode) || "default";
    print "mysql-test-run in $test_mode mode: *** Failing the test(s):";

    foreach my $tinfo (@$tests)
    {
      if ( $tinfo->{'result'} eq 'MTR_RES_FAILED' )
      {
        print " $tinfo->{'name'}";
      }
    }
    print "\n";
  }
  if ( $tot_failed != 0 || $found_problems)
  {
    mtr_error("there where failing test cases");
  }
}

##############################################################################
#
#  Text formatting
#
##############################################################################

sub mtr_print_line () {
  print '-' x 55, "\n";
}

sub mtr_print_thick_line () {
  print '=' x 55, "\n";
}

sub mtr_print_header () {
  print "\n";
  if ( $::opt_timer )
  {
    print "TEST                           RESULT         TIME (ms)\n";
  }
  else
  {
    print "TEST                           RESULT\n";
  }
  mtr_print_line();
  print "\n";
}


##############################################################################
#
#  Misc
#
##############################################################################

sub mtr_report (@) {
  print join(" ", @_),"\n";
}

sub mtr_warning (@) {
  print STDERR "mysql-test-run: WARNING: ",join(" ", @_),"\n";
}

sub mtr_error (@) {
  print STDERR "mysql-test-run: *** ERROR: ",join(" ", @_),"\n";
  mtr_exit(1);
}

sub mtr_child_error (@) {
  print STDERR "mysql-test-run: *** ERROR(child): ",join(" ", @_),"\n";
  exit(1);
}

sub mtr_debug (@) {
  if ( $::opt_script_debug )
  {
    print STDERR "####: ",join(" ", @_),"\n";
  }
}
sub mtr_verbose (@) {
  if ( $::opt_verbose )
  {
    print STDERR "> ",join(" ", @_),"\n";
  }
}

1;
