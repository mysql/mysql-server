# -*- cperl -*-
# Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.
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
		mtr_report_test_skipped mtr_print
		mtr_report_test isotime mtr_summary_file_init mtr_xml_init);

use mtr_match;
use File::Spec;
use My::Platform;
use POSIX qw[ _exit ];
use IO::Handle qw[ flush ];
require "mtr_io.pl";
use mtr_results;

my $tot_real_time= 0;

my $done_percentage= 0;

our $timestamp= 0;
our $timediff= 0;
our $name;
our $verbose;
our $verbose_restart= 0;
our $timer= 1;

our $xml_report_file;
our $summary_report_file;

sub report_option {
  my ($opt, $value)= @_;

  # Evaluate $opt as string to use "Getopt::Long::Callback legacy API"
  my $opt_name = "$opt";

  # Convert - to _ in option name
  $opt_name =~ s/-/_/g;
  no strict 'refs';
  ${$opt_name}= $value;
}

sub _name {
  return $name ? $name." " : undef;
}

sub _mtr_report_test_name ($)
{
  my $tinfo= shift;
  my $tname= $tinfo->{name};

  return unless defined $verbose;

  # Add combination name if any
  $tname.= " '$tinfo->{combination}'" if defined $tinfo->{combination};

  print _name(). _timestamp();
  if($::opt_test_progress)
  {
    printf "[%3s%] %-40s ", $done_percentage, $tname ;
  }
  else
  {
    printf "%-40s ", $tname;
  }

  my $worker = $tinfo->{worker};
  print "w$worker " if defined $worker;

  return $tname;
}


sub mtr_report_test_skipped ($) {
  my ($tinfo)= @_;
  $tinfo->{'result'}= 'MTR_RES_SKIPPED';

  mtr_report_test($tinfo);
}


sub mtr_report_test_passed ($) {
  my ($tinfo)= @_;

  # Save the timer value
  my $timer_str=  "";
  if ( $timer and -f "$::opt_vardir/log/timer" )
  {
    $timer_str= mtr_fromfile("$::opt_vardir/log/timer");
    $tinfo->{timer}= $timer_str;
    resfile_test_info('duration', $timer_str) if $::opt_resfile;
  }

  # Big warning if status already set
  if ( $tinfo->{'result'} ){
    mtr_warning("mtr_report_test_passed: Test result",
		"already set to '", $tinfo->{'result'}, ",");
  }

  $tinfo->{'result'}= 'MTR_RES_PASSED';

  mtr_report_test($tinfo);

  resfile_global("endtime ", isotime (time));
}


sub mtr_report_test ($) {
  my ($tinfo)= @_;

  my $comment=  $tinfo->{'comment'};
  my $logfile=  $tinfo->{'logfile'};
  my $warnings= $tinfo->{'warnings'};
  my $result=   $tinfo->{'result'};
  my $retry=    $tinfo->{'retries'} ? "retry-" : "";

  if ($::opt_test_progress)
  {
    if ($tinfo->{'name'} && !$retry)
    {
      $::remaining= $::remaining - 1;
      $done_percentage = 100 - int (($::remaining * 100) /
                                    ($::num_tests_for_report));
    }
  }

  my $test_name = _mtr_report_test_name($tinfo);

  if ($result eq 'MTR_RES_FAILED'){

    my $timest = format_time();
    my $fail = "fail";

    if ( @$::experimental_test_cases )
    {
      # Find out if this test case is an experimental one, so we can treat
      # the failure as an expected failure instead of a regression.
      for my $exp ( @$::experimental_test_cases ) {
	# Include pattern match for combinations
        if ( $exp ne $test_name && $test_name !~ /^$exp / ) {
          # if the expression is not the name of this test case, but has
          # an asterisk at the end, determine if the characters up to
          # but excluding the asterisk are the same
          if ( $exp ne "" && substr($exp, -1, 1) eq "*" ) {
            my $nexp = substr($exp, 0, length($exp) - 1);
            if ( substr($test_name, 0, length($nexp)) ne $nexp ) {
              # no match, try next entry
              next;
            }
            # if yes, fall through to set the exp-fail status
          } else {
            # no match, try next entry
            next;
          }
        }
        $fail = "exp-fail";
        $tinfo->{exp_fail}= 1;
        last;
      }
    }

    if ( $warnings )
    {
      mtr_report("[ $retry$fail ]  Found warnings/errors in server log file!");
      mtr_report("        Test ended at $timest");
      mtr_report($warnings);
      return;
    }
    my $timeout= $tinfo->{'timeout'};
    if ( $timeout )
    {
      mtr_report("[ $retry$fail ]  timeout after $timeout seconds");
      mtr_report("        Test ended at $timest");
      mtr_report("\n$tinfo->{'comment'}");
      return;
    }
    else
    {
      mtr_report("[ $retry$fail ]\n        Test ended at $timest");
    }

    if ( $logfile )
    {
      # Test failure was detected by test tool and its report
      # about what failed has been saved to file. Display the report.
      mtr_report("\n$logfile\n");
    }
    if ( $comment )
    {
      # The test failure has been detected by mysql-test-run.pl
      # when starting the servers or due to other error, the reason for
      # failing the test is saved in "comment"
      mtr_report("\n$comment\n");
    }

    if ( !$logfile and !$comment )
    {
      # Neither this script or the test tool has recorded info
      # about why the test has failed. Should be debugged.
      mtr_report("\nUnknown result, neither 'comment' or 'logfile' set");
    }
  }
  elsif ($result eq 'MTR_RES_SKIPPED')
  {
    if ( $tinfo->{'disable'} )
    {
      mtr_report("[ disabled ]  $comment");
    }
    elsif ( $comment )
    {
      mtr_report("[ skipped ]  $comment");
    }
    else
    {
      mtr_report("[ skipped ]");
    }
  }
  elsif ($result eq 'MTR_RES_PASSED')
  {
    my $timer_str= $tinfo->{timer} || "";
    $tot_real_time += ($timer_str/1000);
    mtr_report("[ ${retry}pass ] ", sprintf("%5s", $timer_str));

    # Show any problems check-testcase found
    if ( defined $tinfo->{'check'} )
    {
      mtr_report($tinfo->{'check'});
    }
  }
}


sub mtr_generate_xml_report($) {
  my ($tests)= @_;
  my $suite_group;

  # ------------------------------------
  # Calculate suitewise/total statistics
  # ------------------------------------

  my %suite_stats;
  my %total_stats;
  my @stat_list= ('tests', 'failures', 'disabled', 'skipped',
                  'errors', 'unstable', 'time');

  # Initialize overall statistics
  map { $total_stats{$_}= 0 } @stat_list;

  # Sort in order to group tests suitewise
  my @sorted_tests= sort { $a->{'name'} cmp $b->{'name'} } @$tests;

  foreach my $tinfo (@sorted_tests)
  {
    my ($tsuite, $tname) = split /\./, $tinfo->{'name'};

    # Initialize suitewise statistics for new suites
    if ($tsuite ne $suite_group)
    {
      map{ $suite_stats{$tsuite}{$_}= 0 } @stat_list;
      $suite_group= $tsuite;
    }

    # Increment test counters
    $suite_stats{$tsuite}{'tests'}++;
    $total_stats{'tests'}++;

    # Increment result counters based on outcome of the test
    if ($tinfo->{'result'} eq 'MTR_RES_UNSTABLE')
    {
      $suite_stats{$tsuite}{'unstable'}++;
      $total_stats{'unstable'}++;
    }
    elsif ($tinfo->{'result'} eq 'MTR_RES_FAILED' or $tinfo->{failures})
    {
      $suite_stats{$tsuite}{'failures'}++;
      $total_stats{'failures'}++;
    }
    elsif ($tinfo->{'result'} eq 'MTR_RES_SKIPPED')
    {
      if ($tinfo->{'disable'})
      {
        $suite_stats{$tsuite}{'disabled'}++;
        $total_stats{'disabled'}++;
      }
      else
      {
        $suite_stats{$tsuite}{'skipped'}++;
        $total_stats{'skipped'}++;
      }
    }

    # Update duration
    $suite_stats{$tsuite}{'time'}+= $tinfo->{'timer'};
    $total_stats{'time'}+= $tinfo->{'timer'};
  }

  # --------------------------------------
  # Generate xml report from obtained data
  # --------------------------------------

  # Convert overall test duration from ms to s
  $total_stats{'time'}/= 1000.0;

  my $total_unstable_tests= $::opt_report_unstable_tests ?
                            "unstable-tests=\"$total_stats{'unstable'}\" "
                            : "";

  # Top level xml tag
  print $xml_report_file "<testsuites tests=\"$total_stats{'tests'}\" ".
                         "failures=\"$total_stats{'failures'}\" ".
                         "$total_unstable_tests".
                         "disabled=\"$total_stats{'disabled'}\" ".
                         "skipped=\"$total_stats{'skipped'}\" ".
                         "errors=\"$total_stats{'errors'}\" ".
                         "time=\"$total_stats{'time'}\" ".
                         "name=\"AllTests\">\n";

  $suite_group= "";

  foreach my $tinfo (@sorted_tests)
  {
    my ($tsuite, $tname)= split /\./, $tinfo->{'name'};

    # Put reports in a suite-wise manner
    if ($tsuite ne $suite_group)
    {
      if ($suite_group)
      {
        print $xml_report_file " " x 2;
        print $xml_report_file "</testsuite>\n";
      }

      $suite_group= $tsuite;
      my $suite_time= $suite_stats{$tsuite}{'time'} / 1000.0;
      my $suite_unstable_tests= $::opt_report_unstable_tests ?
                                "unstable-tests=\"".
                                $suite_stats{$tsuite}{'unstable'}."\" "
                                : "";

      print $xml_report_file " " x 2;
      print $xml_report_file "<testsuite name=\"$tsuite\" ".
                             "tests=\"$suite_stats{$tsuite}{'tests'}\" ".
                             "failures=\"$suite_stats{$tsuite}{'failures'}\" ".
                             "$suite_unstable_tests".
                             "disabled=\"$suite_stats{$tsuite}{'disabled'}\" ".
                             "skipped=\"$suite_stats{$tsuite}{'skipped'}\" ".
                             "errors=\"$suite_stats{$tsuite}{'errors'}\" ".
                             "time=\"$suite_time\">\n";
    }

    # Get the test's variant(if any)
    my $combination= $tinfo->{combination} ?
                     " combination=\"".$tinfo->{combination}."\"" : "";

    my $test_time= $tinfo->{'timer'} / 1000.0;

    # Print outcome-based tags
    if ($tinfo->{'result'} eq 'MTR_RES_UNSTABLE')
    {
      print $xml_report_file " " x 4;
      print $xml_report_file "<testcase name=\"$tname\"$combination ".
                             "status=\"unstable\" ".
                             "time=\"$test_time\" ".
                             "suitename=\"$tsuite\" >\n";

      print $xml_report_file " " x 7;
      print $xml_report_file "<unstable message=\"Test unstable: It failed ".
                             "initially but passed on one or more retries\" ".
                             "passed=\"".
                             ($tinfo->{retries} - $tinfo->{failures} - 1).
                             "\" failed=\"$tinfo->{failures}\" />\n";

      print $xml_report_file " " x 4;
      print $xml_report_file "</testcase>\n";
    }
    elsif ($tinfo->{'result'} eq 'MTR_RES_FAILED' or $tinfo->{'failures'})
    {
      print $xml_report_file " " x 4;
      print $xml_report_file "<testcase name=\"$tname\"$combination ".
                             "status=\"fail\" ".
                             "time=\"$test_time\" ".
                             "suitename=\"$tsuite\" >\n";

      # Get information regarding the failure
      my $failure_log= $tinfo->{'logfile'};
      my $failure_comment;

      if ($tinfo->{'warnings'})
      {
        $failure_comment= ": Found warnings/errors in server log file!";
        $failure_log= $failure_log."\n".$tinfo->{'warnings'};
      }

      $failure_comment= ": ".$1 if ($failure_log =~ /mysqltest: (.*)/);

      # For large comments from mysql-test-run.pl, display a brief message in
      # the attribute 'message' and prepend details within the <failure> tag
      my @comment_patterns=
      ("MTR's internal check of the test case '.+' failed\\.",
       "Test case timeout after \\d+ seconds",
       "Could not execute 'check-testcase' [a-z]+ testcase '.+' \\(res: .*\\)",
       "Could not execute 'check-warnings' for testcase '.+' \\(res: .*\\)",
       "Server .+ failed during test run",
       "The server .+ crashed while running 'check testcase [a-z]+ test'",
       "The server .+ crashed while running 'check warnings'",
       "mysqltest failed but provided no output");

      my $comment_regex= "(". join('|', @comment_patterns). ")(.*)";
      if ($tinfo->{'comment'}=~ /$comment_regex/s)
      {
        $failure_comment.= ", ".$1;
        $failure_log= $2."\n".$failure_log;
      }
      else
      {
        $failure_comment= ", ".$tinfo->{'comment'} if ($tinfo->{'comment'});
      }

      if (!$failure_log)
      {
        $failure_log= "Failure log not found";
        $failure_comment= ": Unknown result, no comment or log found"
          if (!$failure_comment);
      }

      print $xml_report_file " " x 7;
      print $xml_report_file "<failure message=\"Test failed".
                             "$failure_comment\">\n";

      # Embed failure log as a literal string so that the XML parser
      # ignores any markup which might be included
      my $failure_log_indent= " " x 10;
      my $failure_log_message= "";
      map { $failure_log_message= $failure_log_message.$failure_log_indent.$_ }
          (split /^/, $failure_log);

      print $xml_report_file $failure_log_indent."<![CDATA[\n".
                             $failure_log_message."\n".
                             $failure_log_indent."]]>\n";

      print $xml_report_file " " x 7;
      print $xml_report_file "</failure>\n";

      print $xml_report_file " " x 4;
      print $xml_report_file "</testcase>\n";
    }
    elsif ($tinfo->{'result'} eq 'MTR_RES_SKIPPED')
    {
      # Differentiate disabled and skipped tests as we return
      # the same outcome code for both
      my $not_run_status= $tinfo->{'disable'} ? "disabled" : "skipped";

      print $xml_report_file " " x 4;
      print $xml_report_file "<testcase name=\"$tname\"$combination ".
                             "status=\"$not_run_status\" ".
                             "time=\"$test_time\" ".
                             "suitename=\"$tsuite\" ".
                             "comment=\"$tinfo->{'comment'}\" />\n";
    }
    elsif ($tinfo->{'result'} eq 'MTR_RES_PASSED')
    {
      print $xml_report_file " " x 4;
      print $xml_report_file "<testcase name=\"$tname\"$combination ".
                             "status=\"pass\" ".
                             "time=\"$test_time\" ".
                             "suitename=\"$tsuite\" />\n";
    }
  }
  if ($suite_group)
  {
    print $xml_report_file " " x 2;
    print $xml_report_file "</testsuite>\n";
  }
  print $xml_report_file "</testsuites>\n";
}


sub mtr_report_stats ($$;$) {
  my ($prefix, $tests, $dont_error)= @_;

  # ----------------------------------------------------------------------
  # Find out how we where doing
  # ----------------------------------------------------------------------

  my $tot_skipped= 0;
  my $tot_skipdetect= 0;
  my $tot_passed= 0;
  my $tot_failed= 0;
  my $tot_unstable= 0;
  my $tot_tests=  0;
  my $tot_restarts= 0;
  my $found_problems= 0;

  foreach my $tinfo (@$tests)
  {
    if ( $tinfo->{failures} )
    {
      # Test has failed at least one time
      $tot_tests++;
      $tot_failed++;
      if ($::opt_report_unstable_tests and defined $tinfo->{retries})
      {
        my $num_passed= $tinfo->{retries} - $tinfo->{failures} - 1;

        # Tests which exhibit both passing and failing behaviour
        # with the same code are unstable tests. The level of in-
        # stability is not restricted i.e., a failed test which is
        # successful on at least one retry is marked unstable.
        if ($num_passed > 0)
        {
          $tot_unstable++;
          $tinfo->{'result'}= 'MTR_RES_UNSTABLE';
        }
      }
    }
    elsif ( $tinfo->{'result'} eq 'MTR_RES_SKIPPED' )
    {
      # Test was skipped (disabled not counted)
      $tot_skipped++ unless $tinfo->{'disable'};
      $tot_skipdetect++ if $tinfo->{'skip_detected_by_test'};
    }
    elsif ( $tinfo->{'result'} eq 'MTR_RES_PASSED' )
    {
      # Test passed
      $tot_tests++;
      $tot_passed++;
    }

    if ( $tinfo->{'restarted'} )
    {
      # Servers was restarted
      $tot_restarts++;
    }

    # Add counts for repeated runs, if any.
    # Note that the last run has already been counted above.
    my $num_repeat = $tinfo->{'repeat'} - 1;
    if ( $num_repeat > 0 )
    {
      $tot_tests += $num_repeat;
      my $rep_failed = $tinfo->{'rep_failures'} || 0;
      $tot_failed += $rep_failed;
      $tot_passed += $num_repeat - $rep_failed;
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

  if ( $timer )
  {
    use English;

    mtr_report("Spent", sprintf("%.3f", $tot_real_time),"of",
	       time - $BASETIME, "seconds executing testcases");
  }

  resfile_global("duration", time - $BASETIME) if $::opt_resfile;

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
    my %check_testcases;

    foreach my $tinfo (@$tests)
    {
      if ( defined $tinfo->{'check_testcase_failed'} )
      {
	$check_testcases{$tinfo->{'name'}}= 1;
      }
    }

    if ( keys %check_testcases )
    {
      print "Check of testcase failed for: ";
      print join(" ", keys %check_testcases);
      print "\n\n";
    }
  }

  # Print summary line prefix
  summary_print("$prefix: ");

  # Print a list of testcases that failed
  if ( $tot_failed != 0 )
  {

    # Print each failed test, again
    #foreach my $test ( @$tests ){
    #  if ( $test->{failures} ) {
    #    mtr_report_test($test);
    #  }
    #}

    my $ratio=  $tot_passed * 100 / $tot_tests;
    summary_print(sprintf("Failed $tot_failed/$tot_tests tests, %.2f%% were successful.\n\n", $ratio));

    # Hashes to keep track of reported failures
    my %seen= ();
    my %seen_unstable= ();

    foreach my $tinfo (@$tests)
    {
      my $tname= $tinfo->{'name'};
      if (($tinfo->{failures} || $tinfo->{rep_failures}))
      {
        # Check for unstable tests
        if ($tinfo->{result} eq "MTR_RES_UNSTABLE" and !$seen_unstable{$tname})
        {
          # Report all unstable tests in the format :-
          # <test_name>(<number of failures>/<total attempts>).
          #
          # Marking the test as 'seen' in case of unstable tests might cause
          # hard-failures in other combination runs of the same test to
          # not be reported. Separate hash used to avoid redundant mentions
          # of an unstable test
          $seen_unstable{$tname}= "(".$tinfo->{failures}."/".
                                  ($tinfo->{retries} - 1).")";
        }
        elsif (!$seen{$tname})
        {
          $seen{$tname}= 1;
        }
      }
    }

    # Print the list of tests that failed in a format
    # that can be copy pasted to rerun only failing tests
    if (%seen)
    {
      summary_print("Failing test(s): ".
                    join(" ", keys %seen). "\n\n");
    }

    # Print unstable tests, if any
    if (%seen_unstable)
    {
      summary_print("Unstable test(s)(failures/attempts): ".
                    join(" ", map {$_.$seen_unstable{$_}} keys %seen_unstable).
                    "\n\n");
    }

    # Print info about reporting the error
    print
      "The log files in var/log may give you some hint of what went wrong.\n\n",
      "If you want to report this error, please read first ",
      "the documentation\n",
      "at http://dev.mysql.com/doc/mysql/en/mysql-test-suite.html\n\n";

   }
  else
  {
    summary_print("All $tot_tests tests were successful.\n\n");
  }
  close($summary_report_file) if defined($summary_report_file);

  if ($xml_report_file)
  {
    mtr_generate_xml_report($tests);
    $xml_report_file->flush();
  }

  print "$tot_skipped tests were skipped, ".
    "$tot_skipdetect by the test itself.\n\n" if $tot_skipped;

  if ( $tot_failed != 0 || $found_problems)
  {
    if ($tot_failed == $tot_unstable)
    {
      # Print a warning if all failures are due to unstable tests
      mtr_warning("There are failures due to unstable test cases.\n".
                  "However, the tests are not hard-failing.");
    }
    else
    {
      mtr_error("there were failing test cases") unless $dont_error;
    }
  }
}


##############################################################################
#
#  Text formatting
#
##############################################################################

sub mtr_print_line () {
  print '-' x 74 . "\n";
}


sub mtr_print_thick_line {
  my $char= shift || '=';
  print $char x 78 . "\n";
}


sub mtr_print_header ($) {
  my ($wid) = @_;
  print "\n";
  printf "TEST";
  if ($wid) {
    print " " x 34 . "WORKER ";
  } else {
    print " " x 38;
  }
  print "RESULT   ";
  print "TIME (ms) or " if $timer;
  print "COMMENT\n";
  mtr_print_line();
  print "\n";
}

##############################################################################
#
#  XML Output
#
##############################################################################

sub mtr_xml_init($) {
  my ($fn)= @_;
  # Write xml-report in the build directory for out-of-source builds or current
  # path otherwise, unless an absolute path is already specified
  $fn= File::Spec->rel2abs($fn,
                           $ENV{MTR_BINDIR} ?
                           File::Spec->catfile($ENV{MTR_BINDIR}, "mysql-test"):
                           "");

  unless(open $xml_report_file, '>', $fn) {
    mtr_error("could not create xml_report file $fn");
  }
  print "Writing XML report to $fn...\n";
  print $xml_report_file "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
}

##############################################################################
#
#  Summary file output
#
##############################################################################

sub mtr_summary_file_init($) {
  my ($fn) = @_;

  # For out-of-tree builds, the MTR wrapper script will cd to the source
  # directory. Thus, let's make $summary_report_file local to the vardir
  # instead, if it's not already absolute.
  my $full_path = File::Spec->rel2abs($fn, $::opt_vardir);
  unless(open $summary_report_file, '>', $full_path) {
    mtr_error("could not create summary_report file $fn");
  }
  print "Writing summary report to $fn...\n";
}

sub summary_print($) {
  my ($text) = @_;
  print $text;
  if (defined($summary_report_file)) {
    print $summary_report_file $text;
  }
}

##############################################################################
#
#  Log and reporting functions
#
##############################################################################

use Time::localtime;

use Time::HiRes qw(gettimeofday);

sub format_time {
  my $tm= localtime();
  return sprintf("%4d-%02d-%02d %02d:%02d:%02d",
		 $tm->year + 1900, $tm->mon+1, $tm->mday,
		 $tm->hour, $tm->min, $tm->sec);
}

my $t0= gettimeofday();

sub _timestamp {
  return "" unless $timestamp;

  my $diff;
  if ($timediff){
    my $t1= gettimeofday();
    my $elapsed= $t1 - $t0;

    $diff= sprintf(" +%02.3f", $elapsed);

    # Save current time for next lap
    $t0= $t1;

  }

  my $tm= localtime();
  return sprintf("%02d%02d%02d %2d:%02d:%02d%s ",
		 $tm->year % 100, $tm->mon+1, $tm->mday,
		 $tm->hour, $tm->min, $tm->sec, $diff);
}

# Always print message to screen
sub mtr_print (@) {
  print _name(). join(" ", @_). "\n";
}


# Print message to screen if verbose is defined
sub mtr_report (@) {
  if (defined $verbose)
  {
    print _name(). join(" ", @_). "\n";
  }
}


# Print warning to screen
sub mtr_warning (@) {
  print STDERR _name(). _timestamp().
    "mysql-test-run: WARNING: ". join(" ", @_). "\n";
}


# Print error to screen and then exit
sub mtr_error (@) {
  IO::Handle::flush(\*STDOUT) if IS_WINDOWS;
  print STDERR _name(). _timestamp().
    "mysql-test-run: *** ERROR: ". join(" ", @_). "\n";
  if (IS_WINDOWS)
  {
    POSIX::_exit(1);
  }
  else
  {
    exit(1);
  }
}


sub mtr_debug (@) {
  if ( $verbose > 2 )
  {
    print STDERR _name().
      _timestamp(). "####: ". join(" ", @_). "\n";
  }
}


sub mtr_verbose (@) {
  if ( $verbose )
  {
    print STDERR _name(). _timestamp().
      "> ".join(" ", @_)."\n";
  }
}


sub mtr_verbose_restart (@) {
  my ($server, @args)= @_;
  my $proc= $server->{proc};
  if ( $verbose_restart )
  {
    print STDERR _name()._timestamp().
      "> Restart $proc - ".join(" ", @args)."\n";
  }
}


# Used by --result-file for for formatting times

sub isotime($) {
  my ($sec,$min,$hr,$day,$mon,$yr)= gmtime($_[0]);
  return sprintf "%d-%02d-%02dT%02d:%02d:%02dZ",
    $yr+1900, $mon+1, $day, $hr, $min, $sec;
}

1;
