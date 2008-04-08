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

package mtr_report;
use strict;

use base qw(Exporter);
our @EXPORT= qw(report_option mtr_print_line mtr_print_thick_line
		mtr_print_header mtr_report mtr_report_stats
		mtr_warning mtr_error mtr_debug mtr_verbose
		mtr_verbose_restart mtr_report_test_passed
		mtr_report_test_failed mtr_report_test_skipped
		mtr_report_stats);

use mtr_match;
require "mtr_io.pl";

my $tot_real_time= 0;

our $timestamp= 0;

sub report_option {
  my ($opt, $value)= @_;

  # Convert - to _ in option name
  $opt =~ s/-/_/;
  no strict 'refs';
  ${$opt}= $value;
}

sub SHOW_SUITE_NAME() { return  1; };

sub _mtr_report_test_name ($) {
  my $tinfo= shift;
  my $tname= $tinfo->{name};

  # Remove suite part of name
  $tname =~ s/.*\.// unless SHOW_SUITE_NAME;

  # Add combination name if any
  $tname.= " '$tinfo->{combination}'"
    if defined $tinfo->{combination};

  print _timestamp();
  printf "%-30s ", $tname;
}


sub mtr_report_test_skipped ($) {
  my $tinfo= shift;
  _mtr_report_test_name($tinfo);

  $tinfo->{'result'}= 'MTR_RES_SKIPPED';
  if ( $tinfo->{'disable'} )
  {
    mtr_report("[ disabled ]  $tinfo->{'comment'}");
  }
  elsif ( $tinfo->{'comment'} )
  {
    if ( $tinfo->{skip_detected_by_test} )
    {
      mtr_report("[ skip ].  $tinfo->{'comment'}");
    }
    else
    {
      mtr_report("[ skip ]  $tinfo->{'comment'}");
    }
  }
  else
  {
    mtr_report("[ skip ]");
  }
}


sub mtr_report_test_passed ($$) {
  my ($tinfo, $use_timer)= @_;
  _mtr_report_test_name($tinfo);

  my $timer=  "";
  if ( $use_timer and -f "$::opt_vardir/log/timer" )
  {
    $timer= mtr_fromfile("$::opt_vardir/log/timer");
    $tot_real_time += ($timer/1000);
    $timer= sprintf "%12s", $timer;
  }
  # Set as passed unless already set
  if ( not defined $tinfo->{'result'} ){
    $tinfo->{'result'}= 'MTR_RES_PASSED';
  }
  mtr_report("[ pass ]   $timer");
}


sub mtr_report_test_failed ($$) {
  my ($tinfo, $logfile)= @_;
  _mtr_report_test_name($tinfo);

  $tinfo->{'result'}= 'MTR_RES_FAILED';
  my $test_failures= $tinfo->{'failures'} || 0;
  $tinfo->{'failures'}=  $test_failures + 1;
  if ( defined $tinfo->{'warnings'} )
  {
    mtr_report("[ fail ]  Found warnings in server log file!");
    mtr_report($tinfo->{'warnings'});
    return;
  }
  elsif ( defined $tinfo->{'timeout'} )
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
  elsif ( defined $logfile and -f $logfile )
  {
    # Test failure was detected by test tool and its report
    # about what failed has been saved to file. Display the report.
    print "\n";
    mtr_printfile($logfile);
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
  my $found_problems= 0;

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

    # Look for warnings produced by mysqltest
    my $base_file= mtr_match_extension($tinfo->{'result_file'},
				       "result"); # Trim extension
    my $warning_file= "$base_file.warnings";
    if ( -f $warning_file )
    {
      $found_problems= 1;
      mtr_warning("Check myqltest warnings in '$warning_file'");
    }
  }

  # ----------------------------------------------------------------------
  # Print out a summary report to screen
  # ----------------------------------------------------------------------
  print "The servers were restarted $tot_restarts times\n";

  if ( $::opt_timer )
  {
    use English;

    mtr_report("Spent", sprintf("%.3f", $tot_real_time),"of",
	       time - $BASETIME, "seconds executing testcases");
  }


  my $warnlog= "$::opt_vardir/log/warnings";
  if ( -f $warnlog )
  {
    mtr_warning("Got errors/warnings while running tests, please examine",
		"'$warnlog' for details.");
  }

  print "\n";

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

  # Print a list of testcases that failed
  if ( $tot_failed != 0 )
  {
    my $ratio=  $tot_passed * 100 / $tot_tests;
    print "Failed $tot_failed/$tot_tests tests, ";
    printf("%.2f", $ratio);
    print "\% were successful.\n\n";

    # Print the list of test that failed in a format
    # that can be copy pasted to rerun only failing tests
    print "Failing test(s):";

    my %seen= ();
    foreach my $tinfo (@$tests)
    {
      my $tname= $tinfo->{'name'};
      if ( $tinfo->{'result'} eq 'MTR_RES_FAILED' and ! $seen{$tname})
      {
        print " $tname";
	$seen{$tname}= 1;
      }
    }
    print "\n\n";

    # Print info about reporting the error
    print
      "The log files in var/log may give you some hint of what went wrong.\n\n",
      "If you want to report this error, please read first ",
      "the documentation\n",
      "at http://dev.mysql.com/doc/mysql/en/mysql-test-suite.html\n\n";

   }
  else
  {
    print "All $tot_tests tests were successful.\n";
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
  print '-' x 60, "\n";
}


sub mtr_print_thick_line {
  my $char= shift || '=';
  print $char x 60, "\n";
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
#  Log and reporting functions
#
##############################################################################

use Time::localtime;

sub _timestamp {
  return "" unless $timestamp;

  my $tm= localtime();
  return sprintf("%02d%02d%02d %2d:%02d:%02d ",
		 $tm->year % 100, $tm->mon+1, $tm->mday,
		 $tm->hour, $tm->min, $tm->sec);
}


# Print message to screen
sub mtr_report (@) {
  print join(" ", @_), "\n";
}


# Print warning to screen
sub mtr_warning (@) {
  print STDERR _timestamp(), "mysql-test-run: WARNING: ", join(" ", @_), "\n";
}


# Print error to screen and then exit
sub mtr_error (@) {
  print STDERR _timestamp(), "mysql-test-run: *** ERROR: ", join(" ", @_), "\n";
  exit(1);
}


sub mtr_debug (@) {
  if ( $::opt_verbose > 1 )
  {
    print STDERR _timestamp(), "####: ", join(" ", @_), "\n";
  }
}


sub mtr_verbose (@) {
  if ( $::opt_verbose )
  {
    print STDERR _timestamp(), "> ",join(" ", @_),"\n";
  }
}


sub mtr_verbose_restart (@) {
  my ($server, @args)= @_;
  my $proc= $server->{proc};
  if ( $::opt_verbose_restart )
  {
    print STDERR _timestamp(), "> Restart $proc - ",join(" ", @args),"\n";
  }
}


1;
