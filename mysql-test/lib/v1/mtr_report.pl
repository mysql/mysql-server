# -*- cperl -*-
# Copyright (c) 2004-2006 MySQL AB, 2008 Sun Microsystems, Inc.
# Use is subject to license terms.
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
use warnings;

sub mtr_report_test_name($);
sub mtr_report_test_passed($);
sub mtr_report_test_failed($);
sub mtr_report_test_skipped($);
sub mtr_report_test_not_skipped_though_disabled($);

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

my $tot_real_time= 0;



##############################################################################
#
#  
#
##############################################################################

sub mtr_report_test_name ($) {
  my $tinfo= shift;
  my $tname= $tinfo->{name};

  $tname.= " '$tinfo->{combination}'"
    if defined $tinfo->{combination};

  _mtr_log($tname);
  printf "%-30s ", $tname;
}

sub mtr_report_test_skipped ($) {
  my $tinfo= shift;

  $tinfo->{'result'}= 'MTR_RES_SKIPPED';
  if ( $tinfo->{'disable'} )
  {
    mtr_report("[ disabled ]  $tinfo->{'comment'}");
  }
  elsif ( $tinfo->{'comment'} )
  {
    mtr_report("[ skipped ]   $tinfo->{'comment'}");
  }
  else
  {
    mtr_report("[ skipped ]");
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
    $tot_real_time += ($timer/1000);
    $timer= sprintf "%12s", $timer;
  }
  $tinfo->{'result'}= 'MTR_RES_PASSED';
  mtr_report("[ pass ]   $timer");
}

sub mtr_report_test_failed ($) {
  my $tinfo= shift;

  $tinfo->{'result'}= 'MTR_RES_FAILED';
  if ( defined $tinfo->{'timeout'} )
  {
    mtr_report("[ fail ]  timeout");
    return;
  }
  else
  {
    mtr_report("[ fail ]");
  }

  if ( $tinfo->{'comment'} )
  {
    # The test failure has been detected by mysql-test-run.pl
    # when starting the servers or due to other error, the reason for
    # failing the test is saved in "comment"
    mtr_report("\nERROR: $tinfo->{'comment'}");
  }
  elsif ( -f $::path_timefile )
  {
    # Test failure was detected by test tool and it's report
    # about what failed has been saved to file. Display the report.
    print "\n";
    print mtr_fromfile($::path_timefile); # FIXME print_file() instead
    print "\n";
  }
  else
  {
    # Neither this script or the test tool has recorded info
    # about why the test has failed. Should be debugged.
    mtr_report("\nUnexpected termination, probably when starting mysqld");;
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
      "http://dev.mysql.com/doc/mysql/en/mysql-test-suite.html\n";
  }
  if (!$::opt_extern)
  {
    print "The servers were restarted $tot_restarts times\n";
  }

  if ( $::opt_timer )
  {
    use English;

    mtr_report("Spent", sprintf("%.3f", $tot_real_time),"of",
	       time - $BASETIME, "seconds executing testcases");
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
			    "InnoDB: Error",
			    "^safe_mutex:",
			    "missing DBUG_RETURN",
			    "mysqld: Warning",
			    "allocated at line",
			    "Attempting backtrace", "Assertion .* failed" )
      {
        foreach my $errlog ( sort glob("$::opt_vardir/log/*.err") )
        {
	  my $testname= "";
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
                /Can't open shared library .*\bha_example\b/ or
                /Couldn't load plugin .*\bha_example\b/ or

		# Due to timing issues, it might be that this warning
		# is printed when the server shuts down and the
		# computer is loaded.
		/Forcing close of thread \d+  user: '.*?'/ or

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
		/Slave: Deadlock found/ or
		/Slave: Error .*Unknown table/ or
		/Slave: Error in Write_rows event: / or
		/Slave: Field .* of table .* has no default value/ or
                /Slave: Field .* doesn't have a default value/ or
		/Slave: Query caused different errors on master and slave/ or
		/Slave: Table .* doesn't exist/ or
		/Slave: Table width mismatch/ or
		/Slave: The incident LOST_EVENTS occured on the master/ or
		/Slave: Unknown error.* 1105/ or
		/Slave: Can't drop database.* database doesn't exist/ or
                /Slave SQL:.*(?:Error_code: \d+|Query:.*)/ or
		/Sort aborted/ or
		/Time-out in NDB/ or
		/One can only use the --user.*root/ or
		/Table:.* on (delete|rename)/ or
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
		# Special case for Bug #26402 in show_check.test
		# Question marks are not valid file name parts
		# on Windows platforms. Ignore this error message. 
		/\QCan't find file: '.\test\????????.frm'\E/ or
		# Special case, made as specific as possible, for:
		# Bug #28436: Incorrect position in SHOW BINLOG EVENTS causes
		#             server coredump
		/\QError in Log_event::read_log_event(): 'Sanity check failed', data_len: 258, event_type: 49\E/ or
                /Statement is not safe to log in statement format/ or

                # test case for Bug#bug29807 copies a stray frm into database
                /InnoDB: Error: table `test`.`bug29807` does not exist in the InnoDB internal/ or
                /Cannot find or open table test\/bug29807 from/ or

                # innodb foreign key tests that fail in ALTER or RENAME produce this
                /InnoDB: Error: in ALTER TABLE `test`.`t[12]`/ or
                /InnoDB: Error: in RENAME TABLE table `test`.`t1`/ or
                /InnoDB: Error: table `test`.`t[12]` does not exist in the InnoDB internal/ or

                # Test case for Bug#14233 produces the following warnings:
                /Stored routine 'test'.'bug14233_1': invalid value in column mysql.proc/ or
                /Stored routine 'test'.'bug14233_2': invalid value in column mysql.proc/ or
                /Stored routine 'test'.'bug14233_3': invalid value in column mysql.proc/ or

                # BUG#29839 - lowercase_table3.test: Cannot find table test/T1
                #             from the internal data dictiona
                /Cannot find table test\/BUG29839 from the internal data dictionary/ or
                # BUG#32080 - Excessive warnings on Solaris: setrlimit could not
                #             change the size of core files
                /setrlimit could not change the size of core files to 'infinity'/ or

		# rpl_extrColmaster_*.test, the slave thread produces warnings
		# when it get updates to a table that has more columns on the
		# master
		/Slave: Unknown column 'c7' in 't15' Error_code: 1054/ or
		/Slave: Can't DROP 'c7'.* 1091/ or
		/Slave: Key column 'c6'.* 1072/ or

		# rpl_idempotency.test produces warnings for the slave.
		($testname eq 'rpl.rpl_idempotency' and
		 (/Slave: Can\'t find record in \'t1\' Error_code: 1032/ or
                  /Slave: Cannot add or update a child row: a foreign key constraint fails .* Error_code: 1452/
		 )) or

		# These tests does "kill" on queries, causing sporadic errors when writing to logs
		(($testname eq 'rpl.rpl_skip_error' or
		  $testname eq 'rpl.rpl_err_ignoredtable' or
		  $testname eq 'binlog.binlog_killed_simulate' or
		  $testname eq 'binlog.binlog_killed') and
		 (/Failed to write to mysql\.\w+_log/
		 )) or

		# rpl_bug33931 has deliberate failures
		($testname eq 'rpl.rpl_bug33931' and
		 (/Failed during slave.*thread initialization/
		  )) or

		# rpl_temporary has an error on slave that can be ignored
		($testname eq 'rpl.rpl_temporary' and
		 (/Slave: Can\'t find record in \'user\' Error_code: 1032/
		 )) or

                # Test case for Bug#31590 produces the following error:
                /Out of sort memory; increase server sort buffer size/ or

                # Bug#35161, test of auto repair --myisam-recover
                /able.*_will_crash/ or

                # lowercase_table3 using case sensitive option on
                # case insensitive filesystem (InnoDB error).
                /Cannot find or open table test\/BUG29839 from/ or

                # When trying to set lower_case_table_names = 2
                # on a case sensitive file system. Bug#37402.
                /lower_case_table_names was set to 2, even though your the file system '.*' is case sensitive.  Now setting lower_case_table_names to 0 to avoid future problems./
		)
            {
              next;                       # Skip these lines
            }
	    if ( /CURRENT_TEST: (.*)/ )
	    {
	      $testname= $1;
	    }
            if ( /$pattern/ )
            {
              if ($leak_reports_expected) {
                next;
              }
              $found_problems= 1;
              print WARN basename($errlog) . ": $testname: $_";
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
#  Log and reporting functions
#
##############################################################################

use IO::File;

my $log_file_ref= undef;

sub mtr_log_init ($) {
  my ($filename)= @_;

  mtr_error("Log is already open") if defined $log_file_ref;

  $log_file_ref= IO::File->new($filename, "a") or
    mtr_warning("Could not create logfile $filename: $!");
}

sub _mtr_log (@) {
  print $log_file_ref join(" ", @_),"\n"
    if defined $log_file_ref;
}

sub mtr_report (@) {
  # Print message to screen and log
  _mtr_log(@_);
  print join(" ", @_),"\n";
}

sub mtr_warning (@) {
  # Print message to screen and log
  _mtr_log("WARNING: ", @_);
  print STDERR "mysql-test-run: WARNING: ",join(" ", @_),"\n";
}

sub mtr_error (@) {
  # Print message to screen and log
  _mtr_log("ERROR: ", @_);
  print STDERR "mysql-test-run: *** ERROR: ",join(" ", @_),"\n";
  mtr_exit(1);
}

sub mtr_child_error (@) {
  # Print message to screen and log
  _mtr_log("ERROR(child): ", @_);
  print STDERR "mysql-test-run: *** ERROR(child): ",join(" ", @_),"\n";
  exit(1);
}

sub mtr_debug (@) {
  # Only print if --script-debug is used
  if ( $::opt_script_debug )
  {
    _mtr_log("###: ", @_);
    print STDERR "####: ",join(" ", @_),"\n";
  }
}

sub mtr_verbose (@) {
  # Always print to log, print to screen only when --verbose is used
  _mtr_log("> ",@_);
  if ( $::opt_verbose )
  {
    print STDERR "> ",join(" ", @_),"\n";
  }
}

1;
