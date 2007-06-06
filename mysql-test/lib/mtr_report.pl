# -*- cperl -*-
# Copyright (C) 2004-2006 MySQL AB
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
  my $result_file_name=  shift;

  # The reject and log files have been dumped to
  # to filenames based on the result_file's name
  my $tname= basename($result_file_name);
  $tname=~ s/\..*$//;

  my $reject_file=  "r/$tname.reject";
  my $result_file=  "r/$tname.result";
  my $log_file=     "$::opt_vardir/log/$tname.log";
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
  elsif ( $tinfo->{'comment'} )
  {
    print "[ skipped ]   $tinfo->{'comment'}\n";
  }
  else
  {
    print "[ skipped ]\n";
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
  if ( defined $tinfo->{'timeout'} )
  {
    print "[ fail ]  timeout\n";
    return;
  }
  else
  {
    print "[ fail ]\n";
  }

  if ( $tinfo->{'comment'} )
  {
    print "\nERROR: $tinfo->{'comment'}\n";
  }
  elsif ( -f $::path_timefile )
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
  my $found_problems= 0; # Some warnings in the logfiles are errors...

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
    print "Failed $tot_failed/$tot_tests tests, ";
    printf("%.2f", $ratio);
    print "\% were successful.\n\n";
    print
      "The log files in var/log may give you some hint\n",
      "of what went wrong.\n",
      "If you want to report this error, please read first ",
      "the documentation at\n",
      "http://www.mysql.com/doc/en/MySQL_test_suite.html\n";
  }
  if (!$::opt_extern)
  {
    print "The servers were restarted $tot_restarts times\n";
  }

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
      foreach my $pattern ( "^Warning:",
			    "\\[Warning\\]",
			    "\\[ERROR\\]",
			    "^Error:", "^==.* at 0x",
			    "InnoDB: Warning",
			    "^safe_mutex:",
			    "missing DBUG_RETURN",
			    "mysqld: Warning",
			    "allocated at line",
			    "Attempting backtrace", "Assertion .* failed" )
      {
        foreach my $errlog ( sort glob("$::opt_vardir/log/*.err") )
        {
          unless ( open(ERR, $errlog) )
          {
            mtr_warning("can't read $errlog");
            next;
          }
          my $leak_reports_expected= undef;
          while ( <ERR> )
          {
            # There is a test case that purposely provokes a
            # SAFEMALLOC leak report, even though there is no actual
            # leak. We need to detect this, and ignore the warning in
            # that case.
            if (/Begin safemalloc memory dump:/) {
              $leak_reports_expected= 1;
            } elsif (/End safemalloc memory dump./) {
              $leak_reports_expected= undef;
            }

            # Skip some non fatal warnings from the log files
            if (
		/\"SELECT UNIX_TIMESTAMP\(\)\" failed on master/ or
		/Aborted connection/ or
		/Client requested master to start replication from impossible position/ or
		/Could not find first log file name in binary log/ or
		/Enabling keys got errno/ or
		/Error reading master configuration/ or
		/Error reading packet/ or
		/Event Scheduler/ or
		/Failed to open log/ or
		/Failed to open the existing master info file/ or
		/Forcing shutdown of [0-9]* plugins/ or
		/Got error [0-9]* when reading table/ or
		/Incorrect definition of table/ or
		/Incorrect information in file/ or
		/InnoDB: Warning: we did not need to do crash recovery/ or
		/Invalid \(old\?\) table or database name/ or
		/Lock wait timeout exceeded/ or
		/Log entry on master is longer than max_allowed_packet/ or
                /unknown option '--loose-/ or
                /unknown variable 'loose-/ or
		/You have forced lower_case_table_names to 0 through a command-line option/ or
		/Setting lower_case_table_names=2/ or
		/NDB Binlog:/ or
		/NDB: failed to setup table/ or
		/NDB: only row based binary logging/ or
		/Neither --relay-log nor --relay-log-index were used/ or
		/Query partially completed/ or
		/Slave I.O thread aborted while waiting for relay log/ or
		/Slave SQL thread is stopped because UNTIL condition/ or
		/Slave SQL thread retried transaction/ or
		/Slave \(additional info\)/ or
		/Slave: .*Duplicate column name/ or
		/Slave: .*master may suffer from/ or
		/Slave: According to the master's version/ or
		/Slave: Column [0-9]* type mismatch/ or
		/Slave: Error .* doesn't exist/ or
		/Slave: Error .*Deadlock found/ or
		/Slave: Error .*Unknown table/ or
		/Slave: Error in Write_rows event: / or
		/Slave: Field .* of table .* has no default value/ or
		/Slave: Query caused different errors on master and slave/ or
		/Slave: Table .* doesn't exist/ or
		/Slave: Table width mismatch/ or
		/Slave: The incident LOST_EVENTS occured on the master/ or
		/Slave: Unknown error.* 1105/ or
		/Slave: Can't drop database.* database doesn't exist/ or
		/Sort aborted/ or
		/Time-out in NDB/ or
		/Warning:\s+One can only use the --user.*root/ or
		/Warning:\s+Setting lower_case_table_names=2/ or
		/Warning:\s+Table:.* on (delete|rename)/ or
		/You have an error in your SQL syntax/ or
		/deprecated/ or
		/description of time zone/ or
		/equal MySQL server ids/ or
		/error .*connecting to master/ or
		/error reading log entry/ or
		/lower_case_table_names is set/ or
		/skip-name-resolve mode/ or
		/slave SQL thread aborted/ or
 		/Slave: .*Duplicate entry/ or
                /Statement is not safe to log in statement format/
	       )
            {
              next;                       # Skip these lines
            }
            if ( /$pattern/ )
            {
              if ($leak_reports_expected) {
                next;
              }
              $found_problems= 1;
              print WARN $_;
            }
          }
        }
      }

      if ( $::opt_check_testcases )
      {
        # Look for warnings produced by mysqltest in testname.warnings
        foreach my $test_warning_file
	  ( glob("$::glob_mysql_test_dir/r/*.warnings") )
        {
          $found_problems= 1;
	  print WARN "Check myqltest warnings in $test_warning_file\n";
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

  # Print a list of testcases that failed
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

  # Print a list of check_testcases that failed(if any)
  if ( $::opt_check_testcases )
  {
    my @check_testcases= ();

    foreach my $tinfo (@$tests)
    {
      if ( defined $tinfo->{'check_testcase_failed'} )
      {
	push(@check_testcases, $tinfo->{'name'});
      }
    }

    if ( @check_testcases )
    {
      print "Check of testcase failed for: ";
      print join(" ", @check_testcases);
      print "\n\n";
    }
  }

  if ( $tot_failed != 0 || $found_problems)
  {
    mtr_error("there were failing test cases");
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
