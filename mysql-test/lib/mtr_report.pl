# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub mtr_report_test_name($);
sub mtr_report_test_passed($);
sub mtr_report_test_failed($);
sub mtr_report_test_skipped($);
sub mtr_report_test_disabled($);

sub mtr_show_failed_diff ($);
sub mtr_report_stats ($);
sub mtr_print_line ();
sub mtr_print_thick_line ();
sub mtr_print_header ();
sub mtr_report (@);
sub mtr_warning (@);
sub mtr_error (@);
sub mtr_debug (@);


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
  my $eval_file=    "r/$tname.eval";

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

  if ( -f $reject_file )
  {
    print "Below are the diffs between actual and expected results:\n";
    print "-------------------------------------------------------\n";
    # FIXME check result code?!
    mtr_run("diff",["-c",$result_file,$reject_file], "", "", "", "");
    print "-------------------------------------------------------\n";
    print "Please follow the instructions outlined at\n";
    print "http://www.mysql.com/doc/en/Reporting_mysqltest_bugs.html\n";
    print "to find the reason to this problem and how to report this.\n\n";
  }
}

sub mtr_report_test_name ($) {
  my $tinfo= shift;

  printf "%-31s ", $tinfo->{'name'};
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
    print "[ skipped ]\n";
  }
}

sub mtr_report_test_passed ($) {
  my $tinfo= shift;

  my $timer=  "";
# FIXME
#  if ( $::opt_timer and -f "$::glob_mysql_test_dir/var/log/timer" )
#  {
#    $timer=  `cat var/log/timer`;
#    $timer=  sprintf "%13s", $timer;
#  }
  $tinfo->{'result'}= 'MTR_RES_PASSED';
  print "[ pass ]   $timer\n";
}

sub mtr_report_test_failed ($) {
  my $tinfo= shift;

  $tinfo->{'result'}= 'MTR_RES_FAILED';
  print "[ fail ]\n";

  # FIXME Instead of this test, and meaningless error message in 'else'
  # we should write out into $::path_timefile when the error occurs.
  if ( -f $::path_timefile )
  {
    print "Errors are (from $::path_timefile) :\n";
    print mtr_fromfile($::path_timefile); # FIXME print_file() instead
    print "\n(the last lines may be the most important ones)\n";
  }
  else
  {
    print "Unexpected termination, probably when starting mysqld\n";
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
      "%.2f\% successful.\n\n", $ratio;
    print
      "The log files in var/log may give you some hint\n",
      "of what when wrong.\n",
      "If you want to report this error, please read first ",
      "the documentation at\n",
      "http://www.mysql.com/doc/en/MySQL_test_suite.html\n";
  }

  # ----------------------------------------------------------------------
  # ----------------------------------------------------------------------

  if ( ! $::glob_use_running_server )
  {

    # Report if there was any fatal warnings/errors in the log files
    #
    unlink("$::glob_mysql_test_dir/var/log/warnings");
    unlink("$::glob_mysql_test_dir/var/log/warnings.tmp");
    # Remove some non fatal warnings from the log files

# FIXME what is going on ????? ;-)
#    sed -e 's!Warning:  Table:.* on delete!!g' -e 's!Warning: Setting lower_case_table_names=2!!g' -e 's!Warning: One can only use the --user.*root!!g' \
#        var/log/*.err \
#        | sed -e 's!Warning:  Table:.* on rename!!g' \
#        > var/log/warnings.tmp;
#
#    found_error=0;
#    # Find errors
#    for i in "^Warning:" "^Error:" "^==.* at 0x"
#    do
#      if ( $GREP "$i" var/log/warnings.tmp >> var/log/warnings )
#    {
#        found_error=1
#      }
#    done
#    unlink("$::glob_mysql_test_dir/var/log/warnings.tmp");
#    if ( $found_error=  "1" )
#      {
#      print "WARNING: Got errors/warnings while running tests. Please examine\n"
#      print "$::glob_mysql_test_dir/var/log/warnings for details.\n"
#    }
#  }
  }

  print "\n";

  if ( $tot_failed != 0 )
  {
    print "mysql-test-run: *** Failing the test(s):";

    foreach my $tinfo (@$tests)
    {
      if ( $tinfo->{'result'} eq 'MTR_RES_FAILED' )
      {
        print " $tinfo->{'name'}";
      }
    }
    print "\n";
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
    print "TEST                            RESULT        TIME (ms)\n";
  }
  else
  {
    print "TEST                            RESULT\n";
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
  die "mysql-test-run: *** ERROR: ",join(" ", @_),"\n";
}

sub mtr_debug (@) {
  if ( $::opt_script_debug )
  {
    print STDERR "####: ",join(" ", @_),"\n";
  }
}

1;
