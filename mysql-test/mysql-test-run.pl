#!/usr/bin/perl
# -*- cperl -*-

# Copyright (c) 2004, 2014, Oracle and/or its affiliates.
# Copyright (c) 2009, 2014, Monty Program Ab
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

#
##############################################################################
#
#  mysql-test-run.pl
#
#  Tool used for executing a suite of .test files
#
#  See the "MySQL Test framework manual" for more information
#  http://dev.mysql.com/doc/mysqltest/en/index.html
#
#
##############################################################################

use strict;
use warnings;

BEGIN {
  # Check that mysql-test-run.pl is started from mysql-test/
  unless ( -f "mysql-test-run.pl" )
  {
    print "**** ERROR **** ",
      "You must start mysql-test-run from the mysql-test/ directory\n";
    exit(1);
  }
  # Check that lib exist
  unless ( -d "lib/" )
  {
    print "**** ERROR **** ",
      "Could not find the lib/ directory \n";
    exit(1);
  }
}

BEGIN {
  # Check backward compatibility support
  # By setting the environment variable MTR_VERSION
  # it's possible to use a previous version of
  # mysql-test-run.pl
  my $version= $ENV{MTR_VERSION} || 2;
  if ( $version == 1 )
  {
    print "=======================================================\n";
    print "  WARNING: Using mysql-test-run.pl version 1!  \n";
    print "=======================================================\n";
    # Should use exec() here on *nix but this appears not to work on Windows
    exit(system($^X, "lib/v1/mysql-test-run.pl", @ARGV) >> 8);
  }
  elsif ( $version == 2 )
  {
    # This is the current version, just continue
    ;
  }
  else
  {
    print "ERROR: Version $version of mysql-test-run does not exist!\n";
    exit(1);
  }
}

use lib "lib";

use Cwd ;
use Cwd 'realpath';
use Getopt::Long;
use My::File::Path; # Patched version of File::Path
use File::Basename;
use File::Copy;
use File::Find;
use File::Temp qw/tempdir/;
use File::Spec::Functions qw/splitdir rel2abs/;
use My::Platform;
use My::SafeProcess;
use My::ConfigFactory;
use My::Options;
use My::Find;
use My::SysInfo;
use My::CoreDump;
use mtr_cases;
use mtr_report;
use mtr_match;
use mtr_unique;
use mtr_results;
use IO::Socket::INET;
use IO::Select;

require "lib/mtr_process.pl";
require "lib/mtr_io.pl";
require "lib/mtr_gcov.pl";
require "lib/mtr_gprof.pl";
require "lib/mtr_misc.pl";

$SIG{INT}= sub { mtr_error("Got ^C signal"); };
$SIG{HUP}= sub { mtr_error("Hangup detected on controlling terminal"); };

our $mysql_version_id;
my $mysql_version_extra;
our $glob_mysql_test_dir;
our $basedir;
our $bindir;

our $path_charsetsdir;
our $path_client_bindir;
our $path_client_libdir;
our $path_language;

our $path_current_testlog;
our $path_testlog;

our $default_vardir;
our $opt_vardir;                # Path to use for var/ dir
our $plugindir;
my $path_vardir_trace;          # unix formatted opt_vardir for trace files
my $opt_tmpdir;                 # Path to use for tmp/ dir
my $opt_tmpdir_pid;

my $opt_start;
my $opt_start_dirty;
my $opt_start_exit;
my $start_only;

END {
  if ( defined $opt_tmpdir_pid and $opt_tmpdir_pid == $$ )
  {
    if (!$opt_start_exit)
    {
      # Remove the tempdir this process has created
      mtr_verbose("Removing tmpdir $opt_tmpdir");
      rmtree($opt_tmpdir);
    }
    else
    {
      mtr_warning("tmpdir $opt_tmpdir should be removed after the server has finished");
    }
  }
}

sub env_or_val($$) { defined $ENV{$_[0]} ? $ENV{$_[0]} : $_[1] }

my $path_config_file;           # The generated config file, var/my.cnf

# Visual Studio produces executables in different sub-directories based on the
# configuration used to build them.  To make life easier, an environment
# variable or command-line option may be specified to control which set of
# executables will be used by the test suite.
our $opt_vs_config = $ENV{'MTR_VS_CONFIG'};

my @DEFAULT_SUITES= qw(
    main-
    archive-
    binlog-
    csv-
    federated-
    funcs_1-
    funcs_2-
    handler-
    heap-
    innodb-
    innodb_zip-
    maria-
    optimizer_unfixed_bugs-
    oqgraph-
    parts-
    percona-
    perfschema-
    plugins-
    rpl-
    sphinx-
    sys_vars-
    unit-
    vcol-
  );
my $opt_suites;

our $opt_verbose= 0;  # Verbose output, enable with --verbose
our $exe_patch;
our $exe_mysql;
our $exe_mysql_plugin;
our $exe_mysqladmin;
our $exe_mysqltest;
our $exe_libtool;
our $exe_mysql_embedded;

our $opt_big_test= 0;
our $opt_staging_run= 0;

our @opt_combinations;

our @opt_extra_mysqld_opt;
our @opt_mysqld_envs;

my $opt_stress;

my $opt_compress;
my $opt_ssl;
my $opt_skip_ssl;
my @opt_skip_test_list;
our $opt_ssl_supported;
my $opt_ps_protocol;
my $opt_sp_protocol;
my $opt_cursor_protocol;
my $opt_view_protocol;
my $opt_non_blocking_api;

our $opt_debug;
my $debug_d= "d,*";
my $opt_debug_common;
our $opt_debug_server;
our @opt_cases;                  # The test cases names in argv
our $opt_embedded_server;

# Options used when connecting to an already running server
my %opts_extern;
sub using_extern { return (keys %opts_extern > 0);};

our $opt_fast= 0;
our $opt_force= 0;
our $opt_mem= $ENV{'MTR_MEM'};
our $opt_clean_vardir= $ENV{'MTR_CLEAN_VARDIR'};

our $opt_gcov;
our $opt_gcov_src_dir;
our $opt_gcov_exe= "gcov";
our $opt_gcov_err= "mysql-test-gcov.err";
our $opt_gcov_msg= "mysql-test-gcov.msg";

our $opt_gprof;
our %gprof_dirs;

our $glob_debugger= 0;
our $opt_gdb;
our $opt_client_gdb;
my $opt_boot_gdb;
our $opt_dbx;
our $opt_client_dbx;
my $opt_boot_dbx;
our $opt_ddd;
our $opt_client_ddd;
my $opt_boot_ddd;
our $opt_manual_gdb;
our $opt_manual_lldb;
our $opt_manual_dbx;
our $opt_manual_ddd;
our $opt_manual_debug;
our $opt_debugger;
our $opt_client_debugger;

my $config; # The currently running config
my $current_config_name; # The currently running config file template

our @opt_experimentals;
our $experimental_test_cases= [];

my $baseport;
# $opt_build_thread may later be set from $opt_port_base
my $opt_build_thread= $ENV{'MTR_BUILD_THREAD'} || "auto";
my $opt_port_base= $ENV{'MTR_PORT_BASE'} || "auto";
my $build_thread= 0;

my $opt_record;

our $opt_resfile= $ENV{'MTR_RESULT_FILE'} || 0;

my $opt_skip_core;

our $opt_check_testcases= 1;
my $opt_mark_progress;
my $opt_max_connections;
our $opt_report_times= 0;

my $opt_sleep;

my $opt_testcase_timeout= $ENV{MTR_TESTCASE_TIMEOUT} ||  15; # minutes
my $opt_suite_timeout   = $ENV{MTR_SUITE_TIMEOUT}    || 360; # minutes
my $opt_shutdown_timeout= $ENV{MTR_SHUTDOWN_TIMEOUT} ||  10; # seconds
my $opt_start_timeout   = $ENV{MTR_START_TIMEOUT}    || 180; # seconds

sub suite_timeout { return $opt_suite_timeout * 60; };

my $opt_wait_all;
my $opt_user_args;
my $opt_repeat= 1;
my $opt_retry= 1;
my $opt_retry_failure= env_or_val(MTR_RETRY_FAILURE => 2);
my $opt_reorder= 1;
my $opt_force_restart= 0;

our $opt_user = "root";

our $opt_valgrind= 0;
my $opt_valgrind_mysqld= 0;
my $opt_valgrind_mysqltest= 0;
my @default_valgrind_args= ("--show-reachable=yes");
my @valgrind_args;
my $opt_strace= 0;
my $opt_strace_client;
my @strace_args;
my $opt_valgrind_path;
my $valgrind_reports= 0;
my $opt_callgrind;
my %mysqld_logs;
my $opt_debug_sync_timeout= 300; # Default timeout for WAIT_FOR actions.
my $warn_seconds = 60;

sub testcase_timeout ($) {
  my ($tinfo)= @_;
  if (exists $tinfo->{'case-timeout'}) {
    # Return test specific timeout if *longer* that the general timeout
    my $test_to= $tinfo->{'case-timeout'};
    $test_to*= 10 if $opt_valgrind;
    return $test_to * 60 if $test_to > $opt_testcase_timeout;
  }
  return $opt_testcase_timeout * 60;
}

sub check_timeout ($) { return testcase_timeout($_[0]); }

our $opt_warnings= 1;

our $ndbcluster_enabled= 0;
my $opt_include_ndbcluster= 0;
my $opt_skip_ndbcluster= 0;

my $exe_ndbd;
my $exe_ndbmtd;
my $exe_ndb_mgmd;
my $exe_ndb_waiter;
my $exe_ndb_mgm;

our %mysqld_variables;
our @optional_plugins;

my $source_dist= 0;

my $opt_max_save_core= env_or_val(MTR_MAX_SAVE_CORE => 5);
my $opt_max_save_datadir= env_or_val(MTR_MAX_SAVE_DATADIR => 20);
my $opt_max_test_fail= env_or_val(MTR_MAX_TEST_FAIL => 10);

my $opt_parallel= $ENV{MTR_PARALLEL} || 1;

# lock file to stop tests
my $opt_stop_file= $ENV{MTR_STOP_FILE};
# print messages when test suite is stopped (for buildbot)
my $opt_stop_keep_alive= $ENV{MTR_STOP_KEEP_ALIVE};

select(STDOUT);
$| = 1; # Automatically flush STDOUT

main();


sub main {
  # Default, verbosity on
  report_option('verbose', 0);

  # This is needed for test log evaluation in "gen-build-status-page"
  # in all cases where the calling tool does not log the commands
  # directly before it executes them, like "make test-force-pl" in RPM builds.
  mtr_report("Logging: $0 ", join(" ", @ARGV));

  command_line_setup();

  # --help will not reach here, so now it's safe to assume we have binaries
  My::SafeProcess::find_bin();

  if ( $opt_gcov ) {
    gcov_prepare($basedir . "/" . $opt_gcov_src_dir);
  }

  
  print "vardir: $opt_vardir\n";
  initialize_servers();
  init_timers();

  mtr_report("Checking supported features...");

  executable_setup();

  if (!$opt_suites) {
    $opt_suites= join ',', collect_default_suites(@DEFAULT_SUITES);
  }
  mtr_report("Using suites: $opt_suites") unless @opt_cases;

  # --debug[-common] implies we run debug server
  $opt_debug_server= 1 if $opt_debug || $opt_debug_common;

  if (using_extern())
  {
    # Connect to the running mysqld and find out what it supports
    collect_mysqld_features_from_running_server();
  }
  else
  {
    # Run the mysqld to find out what features are available
    collect_mysqld_features();
    mysql_install_db(default_mysqld(), "$opt_vardir/install.db");
  }
  check_ndbcluster_support();
  check_ssl_support();
  check_debug_support();

  mtr_report("Collecting tests...");
  my $tests= collect_test_cases($opt_reorder, $opt_suites, \@opt_cases, \@opt_skip_test_list);
  mark_time_used('collect');

  #######################################################################
  my $num_tests= @$tests;
  if ( $opt_parallel eq "auto" ) {
    # Try to find a suitable value for number of workers
    my $sys_info= My::SysInfo->new();

    $opt_parallel= $sys_info->num_cpus();
    for my $limit (2000, 1500, 1000, 500){
      $opt_parallel-- if ($sys_info->min_bogomips() < $limit);
    }
    my $max_par= $ENV{MTR_MAX_PARALLEL} || 8;
    $opt_parallel= $max_par if ($opt_parallel > $max_par);
    $opt_parallel= $num_tests if ($opt_parallel > $num_tests);
    $opt_parallel= 1 if (IS_WINDOWS and $sys_info->isvm());
    $opt_parallel= 1 if ($opt_parallel < 1);
    mtr_report("Using parallel: $opt_parallel");
  }
  $ENV{MTR_PARALLEL} = $opt_parallel;

  if ($opt_parallel > 1 && ($opt_start_exit || $opt_stress)) {
    mtr_warning("Parallel cannot be used with --start-and-exit or --stress\n" .
               "Setting parallel to 1");
    $opt_parallel= 1;
  }

  # Create server socket on any free port
  my $server = new IO::Socket::INET
    (
     LocalAddr => 'localhost',
     Proto => 'tcp',
     Listen => $opt_parallel,
    );
  mtr_error("Could not create testcase server port: $!") unless $server;
  my $server_port = $server->sockport();

  if ($opt_resfile) {
    resfile_init("$opt_vardir/mtr-results.txt");
    print_global_resfile();
  }

  # Create child processes
  my %children;
  for my $child_num (1..$opt_parallel){
    my $child_pid= My::SafeProcess::Base::_safe_fork();
    if ($child_pid == 0){
      $server= undef; # Close the server port in child
      $tests= {}; # Don't need the tests list in child

      # Use subdir of var and tmp unless only one worker
      if ($opt_parallel > 1) {
	set_vardir("$opt_vardir/$child_num");
	$opt_tmpdir= "$opt_tmpdir/$child_num";
      }

      init_timers();
      run_worker($server_port, $child_num);
      exit(1);
    }

    $children{$child_pid}= 1;
  }
  #######################################################################

  mtr_report();
  mtr_print_thick_line();
  mtr_print_header($opt_parallel > 1);

  mark_time_used('init');

  my ($prefix, $fail, $completed, $extra_warnings)=
    run_test_server($server, $tests, $opt_parallel);

  exit(0) if $opt_start_exit;

  # Send Ctrl-C to any children still running
  kill("INT", keys(%children));

  if (!IS_WINDOWS) { 
    # Wait for children to exit
    foreach my $pid (keys %children)
    {
      my $ret_pid= waitpid($pid, 0);
      if ($ret_pid != $pid){
        mtr_report("Unknown process $ret_pid exited");
      }
      else {
        delete $children{$ret_pid};
      }
    }
  }

  if ( not @$completed ) {
    mtr_error("Test suite aborted");
  }

  if ( @$completed != $num_tests){

    # Not all tests completed, failure
    mtr_report();
    mtr_report("Only ", int(@$completed), " of $num_tests completed.");
  }

  if ($opt_valgrind) {
    # Create minimalistic "test" for the reporting
    my $tinfo = My::Test->new
      (
       name           => 'valgrind_report',
      );
    # Set dummy worker id to align report with normal tests
    $tinfo->{worker} = 0 if $opt_parallel > 1;
    if ($valgrind_reports) {
      $tinfo->{result}= 'MTR_RES_FAILED';
      $tinfo->{comment}= "Valgrind reported failures at shutdown, see above";
      $tinfo->{failures}= 1;
    } else {
      $tinfo->{result}= 'MTR_RES_PASSED';
    }
    mtr_report_test($tinfo);
    push @$completed, $tinfo;
    ++$num_tests
  }

  mtr_print_line();

  if ( $opt_gcov ) {
    gcov_collect($basedir . "/" . $opt_gcov_src_dir, $opt_gcov_exe,
		 $opt_gcov_msg, $opt_gcov_err);
  }

  print_total_times($opt_parallel) if $opt_report_times;

  mtr_report_stats($prefix, $fail, $completed, $extra_warnings);

  if ( @$completed != $num_tests)
  {
    mtr_error("Not all tests completed (only ". scalar(@$completed) .
              " of $num_tests)");
  }

  remove_vardir_subs() if $opt_clean_vardir;

  exit(0);
}


sub run_test_server ($$$) {
  my ($server, $tests, $childs) = @_;

  my $num_saved_cores= 0;  # Number of core files saved in vardir/log/ so far.
  my $num_saved_datadir= 0;  # Number of datadirs saved in vardir/log/ so far.
  my $num_failed_test= 0; # Number of tests failed so far
  my $test_failure= 0;    # Set true if test suite failed
  my $extra_warnings= []; # Warnings found during server shutdowns

  # Scheduler variables
  my $max_ndb= $ENV{MTR_MAX_NDB} || $childs / 2;
  $max_ndb = $childs if $max_ndb > $childs;
  $max_ndb = 1 if $max_ndb < 1;
  my $num_ndb_tests= 0;

  my $completed= [];
  my %running;
  my $result;
  my $exe_mysqld= find_mysqld($bindir) || ""; # Used as hint to CoreDump

  my $suite_timeout= start_timer(suite_timeout());

  my $s= IO::Select->new();
  $s->add($server);
  while (1) {
    if ($opt_stop_file)
    {
      if (mtr_wait_lock_file($opt_stop_file, $opt_stop_keep_alive))
      {
        # We were waiting so restart timer process
        my $suite_timeout= start_timer(suite_timeout());
      }
    }

    mark_time_used('admin');
    my @ready = $s->can_read(1); # Wake up once every second
    mark_time_idle();
    foreach my $sock (@ready) {
      if ($sock == $server) {
	# New client connected
	my $child= $sock->accept();
	mtr_verbose("Client connected");
	$s->add($child);
	print $child "HELLO\n";
      }
      else {
	my $line= <$sock>;
	if (!defined $line) {
	  # Client disconnected
	  mtr_verbose("Child closed socket");
	  $s->remove($sock);
	  if (--$childs == 0){
	    return ("Completed", $test_failure, $completed, $extra_warnings);
	  }
	  next;
	}
	chomp($line);

	if ($line eq 'TESTRESULT'){
	  $result= My::Test::read_test($sock);

	  # Report test status
	  mtr_report_test($result);

	  if ( $result->is_failed() ) {

	    # Save the workers "savedir" in var/log
	    my $worker_savedir= $result->{savedir};
	    my $worker_savename= basename($worker_savedir);
	    my $savedir= "$opt_vardir/log/$worker_savename";

	    if ($opt_max_save_datadir > 0 &&
		$num_saved_datadir >= $opt_max_save_datadir)
	    {
	      mtr_report(" - skipping '$worker_savedir/'");
	      rmtree($worker_savedir);
	    }
	    else {
	      mtr_report(" - saving '$worker_savedir/' to '$savedir/'");
	      rename($worker_savedir, $savedir);
	      # Move any core files from e.g. mysqltest
	      foreach my $coref (glob("core*"), glob("*.dmp"))
	      {
		mtr_report(" - found '$coref', moving it to '$savedir'");
                move($coref, $savedir);
              }
	      if ($opt_max_save_core > 0) {
		# Limit number of core files saved
		find({ no_chdir => 1,
		       wanted => sub {
			 my $core_file= $File::Find::name;
			 my $core_name= basename($core_file);

			 # Name beginning with core, not ending in .gz
			 if (($core_name =~ /^core/ and $core_name !~ /\.gz$/)
			     or (IS_WINDOWS and $core_name =~ /\.dmp$/)){
                                                       # Ending with .dmp
			   mtr_report(" - found '$core_name'",
				      "($num_saved_cores/$opt_max_save_core)");

			   My::CoreDump->show($core_file, $exe_mysqld, $opt_parallel);

			   if ($num_saved_cores >= $opt_max_save_core) {
			     mtr_report(" - deleting it, already saved",
					"$opt_max_save_core");
			     unlink("$core_file");
			   } else {
			     mtr_compress_file($core_file) unless @opt_cases;
			   }
			   ++$num_saved_cores;
			 }
		       }
		     },
		     $savedir);
	      }
	    }
	    resfile_print_test();
	    $num_saved_datadir++;
	    $num_failed_test++ unless ($result->{retries} ||
                                       $result->{exp_fail});

            $test_failure= 1;
	    if ( !$opt_force ) {
	      # Test has failed, force is off
	      push(@$completed, $result);
	      if ($result->{'dont_kill_server'})
              {
	        print $sock "BYE\n";
	        next;
              }
	      return ("Failure", 1, $completed, $extra_warnings);
	    }
	    elsif ($opt_max_test_fail > 0 and
		   $num_failed_test >= $opt_max_test_fail) {
	      push(@$completed, $result);
	      mtr_report("Too many tests($num_failed_test) failed!",
			 "Terminating...");
	      return ("Too many failed", 1, $completed, $extra_warnings);
	    }
	  }

	  resfile_print_test();
	  # Retry test run after test failure
	  my $retries= $result->{retries} || 2;
	  my $test_has_failed= $result->{failures} || 0;
	  if ($test_has_failed and $retries <= $opt_retry){
	    # Test should be run one more time unless it has failed
	    # too many times already
	    my $tname= $result->{name};
	    my $failures= $result->{failures};
	    if ($opt_retry > 1 and $failures >= $opt_retry_failure){
	      mtr_report("\nTest $tname has failed $failures times,",
			 "no more retries!\n");
	    }
	    else {
	      mtr_report("\nRetrying test $tname, ".
			 "attempt($retries/$opt_retry)...\n");
              #saving the log file as filename.failed in case of retry
              if ( $result->is_failed() ) {
                my $worker_logdir= $result->{savedir};
                my $log_file_name=dirname($worker_logdir)."/".$result->{shortname}.".log";
                rename $log_file_name,$log_file_name.".failed";
              }
	      delete($result->{result});
	      $result->{retries}= $retries+1;
	      $result->write_test($sock, 'TESTCASE');
	      next;
	    }
	  }

	  # Repeat test $opt_repeat number of times
	  my $repeat= $result->{repeat} || 1;
	  # Don't repeat if test was skipped
	  if ($repeat < $opt_repeat && $result->{'result'} ne 'MTR_RES_SKIPPED')
	  {
	    $result->{retries}= 0;
	    $result->{rep_failures}++ if $result->{failures};
	    $result->{failures}= 0;
	    delete($result->{result});
	    $result->{repeat}= $repeat+1;
	    $result->write_test($sock, 'TESTCASE');
	    next;
	  }

	  # Remove from list of running
	  mtr_error("'", $result->{name},"' is not known to be running")
	    unless delete $running{$result->key()};

	  # Update scheduler variables
	  $num_ndb_tests-- if ($result->{ndb_test});

	  # Save result in completed list
	  push(@$completed, $result);

	}
	elsif ($line eq 'START'){
	  ; # Send first test
	}
	elsif ($line eq 'WARNINGS'){
          my $fake_test= My::Test::read_test($sock);
          my $test_list= join (" ", @{$fake_test->{testnames}});
          push @$extra_warnings, $test_list;
          my $report= $fake_test->{'warnings'};
          mtr_report("***Warnings generated in error logs during shutdown ".
                     "after running tests: $test_list\n\n$report");
          $test_failure= 1;
          if ( !$opt_force ) {
            # Test failure due to warnings, force is off
            return ("Warnings in log", 1, $completed, $extra_warnings);
          }
        }
	elsif ($line =~ /^SPENT/) {
	  add_total_times($line);
	}
	elsif ($line eq 'VALGREP' && $opt_valgrind) {
	  $valgrind_reports= 1;
	}
	else {
	  mtr_error("Unknown response: '$line' from client");
	}

	# Find next test to schedule
	# - Try to use same configuration as worker used last time
	# - Limit number of parallel ndb tests

	my $next;
	my $second_best;
	for(my $i= 0; $i <= @$tests; $i++)
	{
	  my $t= $tests->[$i];

	  last unless defined $t;

	  if (run_testcase_check_skip_test($t)){
	    # Move the test to completed list
	    #mtr_report("skip - Moving test $i to completed");
	    push(@$completed, splice(@$tests, $i, 1));

	    # Since the test at pos $i was taken away, next
	    # test will also be at $i -> redo
	    redo;
	  }

	  # Limit number of parallell NDB tests
	  if ($t->{ndb_test} and $num_ndb_tests >= $max_ndb){
	    #mtr_report("Skipping, num ndb is already at max, $num_ndb_tests");
	    next;
	  }

	  # From secondary choices, we prefer to pick a 'long-running' test if
          # possible; this helps avoid getting stuck with a few of those at the
          # end of high --parallel runs, with most workers being idle.
	  if (!defined $second_best ||
              ($t->{'long_test'} && !($tests->[$second_best]{'long_test'}))){
	    #mtr_report("Setting second_best to $i");
	    $second_best= $i;
	  }

	  # Smart allocation of next test within this thread.

	  if ($opt_reorder and $opt_parallel > 1 and defined $result)
	  {
	    my $wid= $result->{worker};
	    # Reserved for other thread, try next
	    next if (defined $t->{reserved} and $t->{reserved} != $wid);
	    if (! defined $t->{reserved})
	    {
	      my $criteria= $t->{criteria};
	      # Reserve similar tests for this worker, but not too many
	      my $maxres= (@$tests - $i) / $opt_parallel + 1;
	      for (my $j= $i+1; $j <= $i + $maxres; $j++)
	      {
		my $tt= $tests->[$j];
		last unless defined $tt;
		last if $tt->{criteria} ne $criteria;
		$tt->{reserved}= $wid;
	      }
	    }
	  }

	  # At this point we have found next suitable test
	  $next= splice(@$tests, $i, 1);
	  last;
	}

	# Use second best choice if no other test has been found
	if (!$next and defined $second_best){
	  #mtr_report("Take second best choice $second_best");
	  mtr_error("Internal error, second best too large($second_best)")
	    if $second_best >  $#$tests;
	  $next= splice(@$tests, $second_best, 1);
	  delete $next->{reserved};
	}

        xterm_stat(scalar(@$tests));

	if ($next) {
	  # We don't need this any more
	  delete $next->{criteria};
	  $next->write_test($sock, 'TESTCASE');
	  $running{$next->key()}= $next;
	  $num_ndb_tests++ if ($next->{ndb_test});
	}
	else {
	  # No more test, tell child to exit
	  #mtr_report("Saying BYE to child");
	  print $sock "BYE\n";
	}
      }
    }

    # ----------------------------------------------------
    # Check if test suite timer expired
    # ----------------------------------------------------
    if ( has_expired($suite_timeout) )
    {
      mtr_report("Test suite timeout! Terminating...");
      return ("Timeout", 1, $completed, $extra_warnings);
    }
  }
}


sub run_worker ($) {
  my ($server_port, $thread_num)= @_;

  $SIG{INT}= sub { exit(1); };
  $SIG{HUP}= sub { exit(1); };

  # Connect to server
  my $server = new IO::Socket::INET
    (
     PeerAddr => 'localhost',
     PeerPort => $server_port,
     Proto    => 'tcp'
    );
  mtr_error("Could not connect to server at port $server_port: $!")
    unless $server;

  # --------------------------------------------------------------------------
  # Set worker name
  # --------------------------------------------------------------------------
  report_option('name',"worker[$thread_num]");

  # --------------------------------------------------------------------------
  # Set different ports per thread
  # --------------------------------------------------------------------------
  set_build_thread_ports($thread_num);

  # --------------------------------------------------------------------------
  # Turn off verbosity in workers, unless explicitly specified
  # --------------------------------------------------------------------------
  report_option('verbose', undef) if ($opt_verbose == 0);

  environment_setup();

  # Read hello from server which it will send when shared
  # resources have been setup
  my $hello= <$server>;

  setup_vardir();
  check_running_as_root();

  if ( using_extern() ) {
    create_config_file_for_extern(%opts_extern);
  }

  # Ask server for first test
  print $server "START\n";

  mark_time_used('init');

  while (my $line= <$server>){
    chomp($line);
    if ($line eq 'TESTCASE'){
      my $test= My::Test::read_test($server);

      # Clear comment and logfile, to avoid
      # reusing them from previous test
      delete($test->{'comment'});
      delete($test->{'logfile'});

      # A sanity check. Should this happen often we need to look at it.
      if (defined $test->{reserved} && $test->{reserved} != $thread_num) {
	my $tres= $test->{reserved};
	mtr_warning("Test reserved for w$tres picked up by w$thread_num");
      }
      $test->{worker} = $thread_num if $opt_parallel > 1;

      run_testcase($test, $server);
      #$test->{result}= 'MTR_RES_PASSED';
      # Send it back, now with results set
      $test->write_test($server, 'TESTRESULT');
      mark_time_used('restart');
    }
    elsif ($line eq 'BYE'){
      mtr_report("Server said BYE");
      # We need to gracefully shut down the servers to see any
      # Valgrind memory leak errors etc. since last server restart.
      if ($opt_warnings) {
        stop_servers(reverse all_servers());
        if(check_warnings_post_shutdown($server)) {
          # Warnings appeared in log file(s) during final server shutdown.
          exit(1);
        }
      }
      else {
        stop_all_servers($opt_shutdown_timeout);
      }
      mark_time_used('restart');
      my $valgrind_reports= 0;
      if ($opt_valgrind_mysqld) {
        $valgrind_reports= valgrind_exit_reports();
	print $server "VALGREP\n" if $valgrind_reports;
      }
      if ( $opt_gprof ) {
	gprof_collect (find_mysqld($bindir), keys %gprof_dirs);
      }
      mark_time_used('admin');
      print_times_used($server, $thread_num);
      exit($valgrind_reports);
    }
    else {
      mtr_error("Could not understand server, '$line'");
    }
  }

  stop_all_servers();

  exit(1);
}


sub ignore_option {
  my ($opt, $value)= @_;
  mtr_report("Ignoring option '$opt'");
}



# Setup any paths that are $opt_vardir related
sub set_vardir {
  ($opt_vardir)= @_;

  $path_vardir_trace= $opt_vardir;
  # Chop off any "c:", DBUG likes a unix path ex: c:/src/... => /src/...
  $path_vardir_trace=~ s/^\w://;

  # Location of my.cnf that all clients use
  $path_config_file= "$opt_vardir/my.cnf";

  $path_testlog=         "$opt_vardir/log/mysqltest.log";
  $path_current_testlog= "$opt_vardir/log/current_test";

}


sub print_global_resfile {
  resfile_global("start_time", isotime $^T);
  resfile_global("user_id", $<);
  resfile_global("embedded-server", $opt_embedded_server ? 1 : 0);
  resfile_global("ps-protocol", $opt_ps_protocol ? 1 : 0);
  resfile_global("sp-protocol", $opt_sp_protocol ? 1 : 0);
  resfile_global("view-protocol", $opt_view_protocol ? 1 : 0);
  resfile_global("cursor-protocol", $opt_cursor_protocol ? 1 : 0);
  resfile_global("ssl", $opt_ssl ? 1 : 0);
  resfile_global("compress", $opt_compress ? 1 : 0);
  resfile_global("parallel", $opt_parallel);
  resfile_global("check-testcases", $opt_check_testcases ? 1 : 0);
  resfile_global("mysqld", \@opt_extra_mysqld_opt);
  resfile_global("debug", $opt_debug ? 1 : 0);
  resfile_global("gcov", $opt_gcov ? 1 : 0);
  resfile_global("gprof", $opt_gprof ? 1 : 0);
  resfile_global("valgrind", $opt_valgrind ? 1 : 0);
  resfile_global("callgrind", $opt_callgrind ? 1 : 0);
  resfile_global("mem", $opt_mem ? 1 : 0);
  resfile_global("tmpdir", $opt_tmpdir);
  resfile_global("vardir", $opt_vardir);
  resfile_global("fast", $opt_fast ? 1 : 0);
  resfile_global("force-restart", $opt_force_restart ? 1 : 0);
  resfile_global("reorder", $opt_reorder ? 1 : 0);
  resfile_global("sleep", $opt_sleep);
  resfile_global("repeat", $opt_repeat);
  resfile_global("user", $opt_user);
  resfile_global("testcase-timeout", $opt_testcase_timeout);
  resfile_global("suite-timeout", $opt_suite_timeout);
  resfile_global("shutdown-timeout", $opt_shutdown_timeout ? 1 : 0);
  resfile_global("warnings", $opt_warnings ? 1 : 0);
  resfile_global("max-connections", $opt_max_connections);
#  resfile_global("default-myisam", $opt_default_myisam ? 1 : 0);
  resfile_global("product", "MySQL");
  # Somewhat hacky code to convert numeric version back to dot notation
  my $v1= int($mysql_version_id / 10000);
  my $v2= int(($mysql_version_id % 10000)/100);
  my $v3= $mysql_version_id % 100;
  resfile_global("version", "$v1.$v2.$v3");
}



sub command_line_setup {
  my $opt_comment;
  my $opt_usage;
  my $opt_list_options;

  # Read the command line options
  # Note: Keep list in sync with usage at end of this file
  Getopt::Long::Configure("pass_through");
  my %options=(
             # Control what engine/variation to run
             'embedded-server'          => \$opt_embedded_server,
             'ps-protocol'              => \$opt_ps_protocol,
             'sp-protocol'              => \$opt_sp_protocol,
             'view-protocol'            => \$opt_view_protocol,
             'cursor-protocol'          => \$opt_cursor_protocol,
             'non-blocking-api'         => \$opt_non_blocking_api,
             'ssl|with-openssl'         => \$opt_ssl,
             'skip-ssl'                 => \$opt_skip_ssl,
             'compress'                 => \$opt_compress,
             'vs-config=s'              => \$opt_vs_config,

	     # Max number of parallel threads to use
	     'parallel=s'               => \$opt_parallel,

             # Config file to use as template for all tests
	     'defaults-file=s'          => \&collect_option,
	     # Extra config file to append to all generated configs
	     'defaults-extra-file=s'    => \&collect_option,

             # Control what test suites or cases to run
             'force+'                   => \$opt_force,
             'with-ndbcluster-only'     => \&collect_option,
             'ndb|include-ndbcluster'   => \$opt_include_ndbcluster,
             'skip-ndbcluster|skip-ndb' => \$opt_skip_ndbcluster,
             'suite|suites=s'           => \$opt_suites,
             'skip-rpl'                 => \&collect_option,
             'skip-test=s'              => \&collect_option,
             'do-test=s'                => \&collect_option,
             'start-from=s'             => \&collect_option,
             'big-test+'                => \$opt_big_test,
	     'combination=s'            => \@opt_combinations,
             'experimental=s'           => \@opt_experimentals,
	     # skip-im is deprecated and silently ignored
	     'skip-im'                  => \&ignore_option,
             'staging-run'              => \$opt_staging_run,

             # Specify ports
	     'build-thread|mtr-build-thread=i' => \$opt_build_thread,
	     'port-base|mtr-port-base=i'       => \$opt_port_base,

             # Test case authoring
             'record'                   => \$opt_record,
             'check-testcases!'         => \$opt_check_testcases,
             'mark-progress'            => \$opt_mark_progress,

             # Extra options used when starting mysqld
             'mysqld=s'                 => \@opt_extra_mysqld_opt,
             'mysqld-env=s'             => \@opt_mysqld_envs,

             # Run test on running server
             'extern=s'                  => \%opts_extern, # Append to hash

             # Debugging
             'debug'                    => \$opt_debug,
             'debug-common'             => \$opt_debug_common,
             'debug-server'             => \$opt_debug_server,
             'gdb'                      => \$opt_gdb,
             'client-gdb'               => \$opt_client_gdb,
             'manual-gdb'               => \$opt_manual_gdb,
             'manual-lldb'              => \$opt_manual_lldb,
	     'boot-gdb'                 => \$opt_boot_gdb,
             'manual-debug'             => \$opt_manual_debug,
             'ddd'                      => \$opt_ddd,
             'client-ddd'               => \$opt_client_ddd,
             'manual-ddd'               => \$opt_manual_ddd,
	     'boot-ddd'                 => \$opt_boot_ddd,
             'dbx'                      => \$opt_dbx,
	     'client-dbx'               => \$opt_client_dbx,
	     'manual-dbx'               => \$opt_manual_dbx,
	     'debugger=s'               => \$opt_debugger,
	     'boot-dbx'                 => \$opt_boot_dbx,
	     'client-debugger=s'        => \$opt_client_debugger,
             'strace'			=> \$opt_strace,
             'strace-client'            => \$opt_strace_client,
             'strace-option=s'          => \@strace_args,
             'max-save-core=i'          => \$opt_max_save_core,
             'max-save-datadir=i'       => \$opt_max_save_datadir,
             'max-test-fail=i'          => \$opt_max_test_fail,

             # Coverage, profiling etc
             'gcov'                     => \$opt_gcov,
             'gcov-src-dir=s'           => \$opt_gcov_src_dir,
             'gprof'                    => \$opt_gprof,
             'valgrind|valgrind-all'    => \$opt_valgrind,
             'valgrind-mysqltest'       => \$opt_valgrind_mysqltest,
             'valgrind-mysqld'          => \$opt_valgrind_mysqld,
             'valgrind-options=s'       => sub {
	       my ($opt, $value)= @_;
	       # Deprecated option unless it's what we know pushbuild uses
	       if ($value eq "--gen-suppressions=all --show-reachable=yes") {
		 push(@valgrind_args, $_) for (split(' ', $value));
		 return;
	       }
	       die("--valgrind-options=s is deprecated. Use ",
		   "--valgrind-option=s, to be specified several",
		   " times if necessary");
	     },
             'valgrind-option=s'        => \@valgrind_args,
             'valgrind-path=s'          => \$opt_valgrind_path,
	     'callgrind'                => \$opt_callgrind,
	     'debug-sync-timeout=i'     => \$opt_debug_sync_timeout,

	     # Directories
             'tmpdir=s'                 => \$opt_tmpdir,
             'vardir=s'                 => \$opt_vardir,
             'mem'                      => \$opt_mem,
	     'clean-vardir'             => \$opt_clean_vardir,
             'client-bindir=s'          => \$path_client_bindir,
             'client-libdir=s'          => \$path_client_libdir,

             # Misc
             'comment=s'                => \$opt_comment,
             'fast'                     => \$opt_fast,
	     'force-restart'            => \$opt_force_restart,
             'reorder!'                 => \$opt_reorder,
             'enable-disabled'          => \&collect_option,
             'verbose+'                 => \$opt_verbose,
             'verbose-restart'          => \&report_option,
             'sleep=i'                  => \$opt_sleep,
             'start-dirty'              => \$opt_start_dirty,
             'start-and-exit'           => \$opt_start_exit,
             'start'                    => \$opt_start,
	     'user-args'                => \$opt_user_args,
             'wait-all'                 => \$opt_wait_all,
	     'print-testcases'          => \&collect_option,
	     'repeat=i'                 => \$opt_repeat,
	     'retry=i'                  => \$opt_retry,
	     'retry-failure=i'          => \$opt_retry_failure,
             'timer!'                   => \&report_option,
             'user=s'                   => \$opt_user,
             'testcase-timeout=i'       => \$opt_testcase_timeout,
             'suite-timeout=i'          => \$opt_suite_timeout,
             'shutdown-timeout=i'       => \$opt_shutdown_timeout,
             'warnings!'                => \$opt_warnings,
	     'timestamp'                => \&report_option,
	     'timediff'                 => \&report_option,
             'stop-file=s'              => \$opt_stop_file,
             'stop-keep-alive=i'        => \$opt_stop_keep_alive,
	     'max-connections=i'        => \$opt_max_connections,
	     'default-myisam!'          => \&collect_option,
	     'report-times'             => \$opt_report_times,
	     'result-file'              => \$opt_resfile,
	     'stress=s'                 => \$opt_stress,

             'help|h'                   => \$opt_usage,
	     # list-options is internal, not listed in help
	     'list-options'             => \$opt_list_options,
             'skip-test-list=s'         => \@opt_skip_test_list
           );

  GetOptions(%options) or usage("Can't read options");
  usage("") if $opt_usage;
  list_options(\%options) if $opt_list_options;

  # --------------------------------------------------------------------------
  # Setup verbosity
  # --------------------------------------------------------------------------
  if ($opt_verbose != 0){
    report_option('verbose', $opt_verbose);
  }

  if ( -d "../sql" )
  {
    $source_dist=  1;
  }

  # Find the absolute path to the test directory
  $glob_mysql_test_dir= cwd();
  if ($glob_mysql_test_dir =~ / /)
  {
    die("Working directory \"$glob_mysql_test_dir\" contains space\n".
	"Bailing out, cannot function properly with space in path");
  }
  if (IS_CYGWIN)
  {
    # Use mixed path format i.e c:/path/to/
    $glob_mysql_test_dir= mixed_path($glob_mysql_test_dir);
  }

  # In most cases, the base directory we find everything relative to,
  # is the parent directory of the "mysql-test" directory. For source
  # distributions, TAR binary distributions and some other packages.
  $basedir= dirname($glob_mysql_test_dir);

  # In the RPM case, binaries and libraries are installed in the
  # default system locations, instead of having our own private base
  # directory. And we install "/usr/share/mysql-test". Moving up one
  # more directory relative to "mysql-test" gives us a usable base
  # directory for RPM installs.
  if ( ! $source_dist and ! -d "$basedir/bin" )
  {
    $basedir= dirname($basedir);
  }
  # For .deb, it's like RPM, but installed in /usr/share/mysql/mysql-test.
  # So move up one more directory level yet.
  if ( ! $source_dist and ! -d "$basedir/bin" )
  {
    $basedir= dirname($basedir);
  }

  # Respect MTR_BINDIR variable, which is typically set in to the 
  # build directory in out-of-source builds.
  $bindir=$ENV{MTR_BINDIR}||$basedir;
  
  fix_vs_config_dir();

  # Respect MTR_BINDIR variable, which is typically set in to the 
  # build directory in out-of-source builds.
  $bindir=$ENV{MTR_BINDIR}||$basedir;
  
  # Look for the client binaries directory
  if ($path_client_bindir)
  {
    # --client-bindir=path set on command line, check that the path exists
    $path_client_bindir= mtr_path_exists($path_client_bindir);
  }
  else
  {
    $path_client_bindir= mtr_path_exists("$bindir/client_release",
					 "$bindir/client_debug",
					 "$bindir/client$opt_vs_config",
					 "$bindir/client",
					 "$bindir/bin");
  }

  # Look for language files and charsetsdir, use same share
  $path_language=   mtr_path_exists("$bindir/share/mariadb",
                                    "$bindir/share/mysql",
                                    "$bindir/sql/share",
                                    "$bindir/share");
  my $path_share= $path_language;
  $path_charsetsdir =   mtr_path_exists("$basedir/share/mariadb/charsets",
                                    "$basedir/share/mysql/charsets",
                                    "$basedir/sql/share/charsets",
                                    "$basedir/share/charsets");
  if ( $opt_comment )
  {
    mtr_report();
    mtr_print_thick_line('#');
    mtr_report("# $opt_comment");
    mtr_print_thick_line('#');
  }

  if ( @opt_experimentals )
  {
    # $^O on Windows considered not generic enough
    my $plat= (IS_WINDOWS) ? 'windows' : $^O;

    # read the list of experimental test cases from the files specified on
    # the command line
    $experimental_test_cases = [];
    foreach my $exp_file (@opt_experimentals)
    {
      open(FILE, "<", $exp_file)
	or mtr_error("Can't read experimental file: $exp_file");
      mtr_report("Using experimental file: $exp_file");
      while(<FILE>) {
	chomp;
	# remove comments (# foo) at the beginning of the line, or after a 
	# blank at the end of the line
	s/(\s+|^)#.*$//;
	# If @ platform specifier given, use this entry only if it contains
	# @<platform> or @!<xxx> where xxx != platform
	if (/\@.*/)
	{
	  next if (/\@!$plat/);
	  next unless (/\@$plat/ or /\@!/);
	  # Then remove @ and everything after it
	  s/\@.*$//;
	}
	# remove whitespace
	s/^\s+//;
	s/\s+$//;
	# if nothing left, don't need to remember this line
	if ( $_ eq "" ) {
	  next;
	}
	# remember what is left as the name of another test case that should be
	# treated as experimental
	print " - $_\n";
	push @$experimental_test_cases, $_;
      }
      close FILE;
    }
  }

  foreach my $arg ( @ARGV )
  {
    if ( $arg =~ /^--skip-/ )
    {
      push(@opt_extra_mysqld_opt, $arg);
    }
    elsif ( $arg =~ /^--$/ )
    {
      # It is an effect of setting 'pass_through' in option processing
      # that the lone '--' separating options from arguments survives,
      # simply ignore it.
    }
    elsif ( $arg =~ /^-/ )
    {
      usage("Invalid option \"$arg\"");
    }
    else
    {
      push(@opt_cases, $arg);
    }
  }

  if ( @opt_cases )
  {
    # Run big tests if explicitely specified on command line
    $opt_big_test= 1;
  }

  # --------------------------------------------------------------------------
  # Find out type of logging that are being used
  # --------------------------------------------------------------------------
  foreach my $arg ( @opt_extra_mysqld_opt )
  {
    if ( $arg =~ /binlog[-_]format=(\S+)/ )
    {
      # Save this for collect phase
      collect_option('binlog-format', $1);
      mtr_report("Using binlog format '$1'");
    }
  }


  # --------------------------------------------------------------------------
  # Find out default storage engine being used(if any)
  # --------------------------------------------------------------------------
  foreach my $arg ( @opt_extra_mysqld_opt )
  {
    if ( $arg =~ /default-storage-engine=(\S+)/ )
    {
      # Save this for collect phase
      collect_option('default-storage-engine', $1);
      mtr_report("Using default engine '$1'")
    }
  }

  if (IS_WINDOWS and defined $opt_mem) {
    mtr_report("--mem not supported on Windows, ignored");
    $opt_mem= undef;
  }

  if ($opt_port_base ne "auto")
  {
    if (my $rem= $opt_port_base % 10)
    {
      mtr_warning ("Port base $opt_port_base rounded down to multiple of 10");
      $opt_port_base-= $rem;
    }
    $opt_build_thread= $opt_port_base / 10 - 1000;
  }

  # --------------------------------------------------------------------------
  # Check if we should speed up tests by trying to run on tmpfs
  # --------------------------------------------------------------------------
  if ( defined $opt_mem)
  {
    mtr_error("Can't use --mem and --vardir at the same time ")
      if $opt_vardir;
    mtr_error("Can't use --mem and --tmpdir at the same time ")
      if $opt_tmpdir;

    # Search through list of locations that are known
    # to be "fast disks" to find a suitable location
    # Use --mem=<dir> as first location to look.
    my @tmpfs_locations= ($opt_mem,"/run/shm", "/dev/shm", "/tmp");

    foreach my $fs (@tmpfs_locations)
    {
      if ( -d $fs )
      {
	my $template= "var_${opt_build_thread}_XXXX";
	$opt_mem= tempdir( $template, DIR => $fs, CLEANUP => 0);
	last;
      }
    }
  }

  # --------------------------------------------------------------------------
  # Set the "var/" directory, the base for everything else
  # --------------------------------------------------------------------------
  my $vardir_location= (defined $ENV{MTR_BINDIR} 
                          ? "$ENV{MTR_BINDIR}/mysql-test" 
                          : $glob_mysql_test_dir);
  $vardir_location= realpath $vardir_location unless IS_WINDOWS;
  $default_vardir= "$vardir_location/var";

  if ( ! $opt_vardir )
  {
    $opt_vardir= $default_vardir;
  }

  # We make the path absolute, as the server will do a chdir() before usage
  unless ( $opt_vardir =~ m,^/, or
           (IS_WINDOWS and $opt_vardir =~ m,^[a-z]:[/\\],i) )
  {
    # Make absolute path, relative test dir
    $opt_vardir= "$glob_mysql_test_dir/$opt_vardir";
  }

  set_vardir($opt_vardir);

  # --------------------------------------------------------------------------
  # Set the "tmp" directory
  # --------------------------------------------------------------------------
  if ( ! $opt_tmpdir )
  {
    $opt_tmpdir=       "$opt_vardir/tmp" unless $opt_tmpdir;

    if (check_socket_path_length("$opt_tmpdir/mysql_testsocket.sock"))
    {
      mtr_report("Too long tmpdir path '$opt_tmpdir'",
		 " creating a shorter one...");

      # Create temporary directory in standard location for temporary files
      $opt_tmpdir= tempdir( TMPDIR => 1, CLEANUP => 0 );
      mtr_report(" - using tmpdir: '$opt_tmpdir'\n");

      # Remember pid that created dir so it's removed by correct process
      $opt_tmpdir_pid= $$;
    }
  }
  $opt_tmpdir =~ s,/+$,,;       # Remove ending slash if any

  # --------------------------------------------------------------------------
  # fast option
  # --------------------------------------------------------------------------
  if ($opt_fast){
    $opt_shutdown_timeout= 0; # Kill processes instead of nice shutdown
  }

  # --------------------------------------------------------------------------
  # Check parallel value
  # --------------------------------------------------------------------------
  if ($opt_parallel ne "auto" && $opt_parallel < 1)
  {
    mtr_error("0 or negative parallel value makes no sense, use 'auto' or positive number");
  }

  # --------------------------------------------------------------------------
  # Record flag
  # --------------------------------------------------------------------------
  if ( $opt_record and ! @opt_cases )
  {
    mtr_error("Will not run in record mode without a specific test case");
  }

  if ( $opt_record ) {
    # Use only one worker with --record
    $opt_parallel= 1;
  }

  # --------------------------------------------------------------------------
  # Embedded server flag
  # --------------------------------------------------------------------------
  if ( $opt_embedded_server )
  {
    $opt_skip_ssl= 1;              # Turn off use of SSL

    # Turn off use of bin log
    push(@opt_extra_mysqld_opt, "--skip-log-bin");

    if ( using_extern() )
    {
      mtr_error("Can't use --extern with --embedded-server");
    }


    if ($opt_gdb)
    {
      $opt_client_gdb= $opt_gdb;
      $opt_gdb= undef;
    }

    if ($opt_ddd)
    {
      $opt_client_ddd= $opt_ddd;
      $opt_ddd= undef;
    }

    if ($opt_dbx) {
      mtr_warning("Silently converting --dbx to --client-dbx in embedded mode");
      $opt_client_dbx= $opt_dbx;
      $opt_dbx= undef;
    }

    if ($opt_debugger)
    {
      $opt_client_debugger= $opt_debugger;
      $opt_debugger= undef;
    }

    if ( $opt_gdb || $opt_ddd || $opt_manual_gdb || $opt_manual_lldb || 
         $opt_manual_ddd || $opt_manual_debug || $opt_debugger || $opt_dbx || 
         $opt_manual_dbx)
    {
      mtr_error("You need to use the client debug options for the",
		"embedded server. Ex: --client-gdb");
    }
  }

  # --------------------------------------------------------------------------
  # Big test and staging_run flags
  # --------------------------------------------------------------------------
   if ( $opt_big_test )
   {
     $ENV{'BIG_TEST'}= 1;
   }
  $ENV{'STAGING_RUN'}= $opt_staging_run;

  # --------------------------------------------------------------------------
  # Gcov flag
  # --------------------------------------------------------------------------
  if ( ($opt_gcov or $opt_gprof) and ! $source_dist )
  {
    mtr_error("Coverage test needs the source - please use source dist");
  }

  # --------------------------------------------------------------------------
  # Check debug related options
  # --------------------------------------------------------------------------
  if ( $opt_gdb || $opt_client_gdb || $opt_ddd || $opt_client_ddd || 
       $opt_manual_gdb || $opt_manual_lldb || $opt_manual_ddd || 
       $opt_manual_debug || $opt_dbx || $opt_client_dbx || $opt_manual_dbx || 
       $opt_debugger || $opt_client_debugger )
  {
    if ( using_extern() )
    {
      mtr_error("Can't use --extern when using debugger");
    }
    # Indicate that we are using debugger
    $glob_debugger= 1;
    $opt_retry= 1;
    $opt_retry_failure= 1;
    # Set one week timeout (check-testcase timeout will be 1/10th)
    $opt_testcase_timeout= 7 * 24 * 60;
    $opt_suite_timeout= 7 * 24 * 60;
    # One day to shutdown
    $opt_shutdown_timeout= 24 * 60;
    # One day for PID file creation (this is given in seconds not minutes)
    $opt_start_timeout= 24 * 60 * 60;
  }

  # --------------------------------------------------------------------------
  # Modified behavior with --start options
  # --------------------------------------------------------------------------
  if ($opt_start or $opt_start_dirty or $opt_start_exit) {
    collect_option ('quick-collect', 1);
    $start_only= 1;
  }
  if ($opt_debug)
  {
    $opt_testcase_timeout= 7 * 24 * 60;
    $opt_suite_timeout= 7 * 24 * 60;
    $opt_retry= 1;
    $opt_retry_failure= 1;
  }

  # --------------------------------------------------------------------------
  # Check use of user-args
  # --------------------------------------------------------------------------

  if ($opt_user_args) {
    mtr_error("--user-args only valid with --start options")
      unless $start_only;
    mtr_error("--user-args cannot be combined with named suites or tests")
      if $opt_suites || @opt_cases;
  }

  # --------------------------------------------------------------------------
  # Check use of wait-all
  # --------------------------------------------------------------------------

  if ($opt_wait_all && ! $start_only)
  {
    mtr_error("--wait-all can only be used with --start options");
  }

  # --------------------------------------------------------------------------
  # Gather stress-test options and modify behavior
  # --------------------------------------------------------------------------

  if ($opt_stress)
  {
    $opt_stress=~ s/,/ /g;
    $opt_user_args= 1;
    mtr_error("--stress cannot be combined with named ordinary suites or tests")
      if $opt_suites || @opt_cases;
    $opt_suites="stress";
    @opt_cases= ("wrapper");
    $ENV{MST_OPTIONS}= $opt_stress;
  }

  # --------------------------------------------------------------------------
  # Check timeout arguments
  # --------------------------------------------------------------------------

  mtr_error("Invalid value '$opt_testcase_timeout' supplied ".
	    "for option --testcase-timeout")
    if ($opt_testcase_timeout <= 0);
  mtr_error("Invalid value '$opt_suite_timeout' supplied ".
	    "for option --testsuite-timeout")
    if ($opt_suite_timeout <= 0);

  # --------------------------------------------------------------------------
  # Check valgrind arguments
  # --------------------------------------------------------------------------
  if ( $opt_valgrind or $opt_valgrind_path or @valgrind_args)
  {
    mtr_report("Turning on valgrind for all executables");
    $opt_valgrind= 1;
    $opt_valgrind_mysqld= 1;
    $opt_valgrind_mysqltest= 1;
  }
  elsif ( $opt_valgrind_mysqld )
  {
    mtr_report("Turning on valgrind for mysqld(s) only");
    $opt_valgrind= 1;
  }
  elsif ( $opt_valgrind_mysqltest )
  {
    mtr_report("Turning on valgrind for mysqltest and mysql_client_test only");
    $opt_valgrind= 1;
  }

  if ($opt_valgrind)
  {
    # Increase the timeouts when running with valgrind
    $opt_testcase_timeout*= 10;
    $opt_suite_timeout*= 6;
    $opt_start_timeout*= 10;
    $warn_seconds*= 10;
  }

  if ( $opt_callgrind )
  {
    mtr_report("Turning on valgrind with callgrind for mysqld(s)");
    $opt_valgrind= 1;
    $opt_valgrind_mysqld= 1;

    # Set special valgrind options unless options passed on command line
    push(@valgrind_args, "--trace-children=yes")
      unless @valgrind_args;
  }

  if ( $opt_valgrind )
  {
    # Set valgrind_options to default unless already defined
    push(@valgrind_args, @default_valgrind_args)
      unless @valgrind_args;

    # Make valgrind run in quiet mode so it only print errors
    push(@valgrind_args, "--quiet" );

    mtr_report("Running valgrind with options \"",
	       join(" ", @valgrind_args), "\"");
  }

  if (@strace_args)
  {
    $opt_strace=1;
  }

  # InnoDB does not bother to do individual de-allocations at exit. Instead it
  # relies on a custom allocator to track every allocation, and frees all at
  # once during exit.
  # In XtraDB, an option use-sys-malloc is introduced (and on by default) to
  # disable this (for performance). But this exposes Valgrind to all the
  # missing de-allocations, so we need to disable it to at least get
  # meaningful leak checking for the rest of the server.
  if ($opt_valgrind_mysqld)
  {
    push(@opt_extra_mysqld_opt, "--loose-skip-innodb-use-sys-malloc");
  }

  if ($opt_debug_common)
  {
    $opt_debug= 1;
    $debug_d= "d,query,info,error,enter,exit";
  }
}


#
# To make it easier for different devs to work on the same host,
# an environment variable can be used to control all ports. A small
# number is to be used, 0 - 16 or similar.
#
# Note the MASTER_MYPORT has to be set the same in all 4.x and 5.x
# versions of this script, else a 4.0 test run might conflict with a
# 5.1 test run, even if different MTR_BUILD_THREAD is used. This means
# all port numbers might not be used in this version of the script.
#
# Also note the limitation of ports we are allowed to hand out. This
# differs between operating systems and configuration, see
# http://www.ncftp.com/ncftpd/doc/misc/ephemeral_ports.html
# But a fairly safe range seems to be 5001 - 32767
#
sub set_build_thread_ports($) {
  my $thread= shift || 0;

  if ( lc($opt_build_thread) eq 'auto' ) {
    my $found_free = 0;
    $build_thread = 300;	# Start attempts from here
    my $build_thread_upper = $build_thread + ($opt_parallel > 1500
                                              ? 3000
                                              : 2 * $opt_parallel) + 300;
    while (! $found_free)
    {
      $build_thread= mtr_get_unique_id($build_thread, $build_thread_upper);
      if ( !defined $build_thread ) {
        mtr_error("Could not get a unique build thread id");
      }
      $found_free= check_ports_free($build_thread);
      # If not free, release and try from next number
      if (! $found_free) {
        mtr_release_unique_id();
        $build_thread++;
      }
    }
  }
  else
  {
    $build_thread = $opt_build_thread + $thread - 1;
    if (! check_ports_free($build_thread)) {
      # Some port was not free(which one has already been printed)
      mtr_error("Some port(s) was not free")
    }
  }
  $ENV{MTR_BUILD_THREAD}= $build_thread;

  # Calculate baseport
  $baseport= $build_thread * 20 + 10000;
  if ( $baseport < 5001 or $baseport + 19 >= 32767 )
  {
    mtr_error("MTR_BUILD_THREAD number results in a port",
              "outside 5001 - 32767",
              "($baseport - $baseport + 19)");
  }

  mtr_report("Using MTR_BUILD_THREAD $build_thread,",
	     "with reserved ports $baseport..".($baseport+19));

}


sub collect_mysqld_features {
  #
  # Execute "mysqld --no-defaults --help --verbose" to get a
  # list of all features and settings
  #
  # --no-defaults and --skip-grant-tables are to avoid loading
  # system-wide configs and plugins
  #
  # --datadir must exist, mysqld will chdir into it
  #
  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--datadir=.");
  mtr_add_arg($args, "--basedir=%s", $basedir);
  mtr_add_arg($args, "--lc-messages-dir=%s", $path_language);
  mtr_add_arg($args, "--skip-grant-tables");
  mtr_add_arg($args, "--log-warnings=0");
  mtr_add_arg($args, "--verbose");
  mtr_add_arg($args, "--help");

  my $exe_mysqld= find_mysqld($bindir);
  my $cmd= join(" ", $exe_mysqld, @$args);

  mtr_verbose("cmd: $cmd");

  my $list= `$cmd`;

  # to simplify the parsing, we'll merge all nicely formatted --help texts
  $list =~ s/\n {22}(\S)/ $1/g;

  my @list= split '\n', $list;
  mtr_error("Could not find version of MariaDB")
     unless shift(@list) =~ /^\Q$exe_mysqld\E\s+Ver\s(\d+)\.(\d+)\.(\d+)(\S*)/;
  $mysql_version_id= $1*10000 + $2*100 + $3;
  $mysql_version_extra= $4;
  mtr_report("MariaDB Version $1.$2.$3$4");

  for (@list)
  {
    # first part of the help - command-line options.
    if (/Copyright/ .. /^-{30,}/) {
      # here we want to detect all not mandatory plugins
      # they are listed in the --help output as
      #  --archive[=name]    Enable or disable ARCHIVE plugin. Possible values are ON, OFF, FORCE (don't start if the plugin fails to load).
      push @optional_plugins, $1
        if /^  --([-a-z0-9]+)\[=name\] +Enable or disable \w+ plugin. Possible values are ON, OFF, FORCE/;
      next;
    }

    last if /^$/; # then goes a list of variables, it ends with an empty line

    # Put a variable into hash
    /^([\S]+)[ \t]+(.*?)\r?$/ or die "Could not parse mysqld --help: $_\n";
    $mysqld_variables{$1}= $2;
  }
  mtr_error("Could not find variabes list") unless %mysqld_variables;
}



sub collect_mysqld_features_from_running_server ()
{
  my $mysql= mtr_exe_exists("$path_client_bindir/mysql");

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--user=%s", $opt_user);

  while (my ($option, $value)= each( %opts_extern )) {
    mtr_add_arg($args, "--$option=$value");
  }

  mtr_add_arg($args, "--silent"); # Tab separated output
  mtr_add_arg($args, "-e '%s'", "use mysql; SHOW VARIABLES");
  my $cmd= "$mysql " . join(' ', @$args);
  mtr_verbose("cmd: $cmd");

  my $list = `$cmd` or
    mtr_error("Could not connect to extern server using command: '$cmd'");
  foreach my $line (split('\n', $list ))
  {
    # Put variables into hash
    if ( $line =~ /^([\S]+)[ \t]+(.*?)\r?$/ )
    {
      my $name= $1;
      my $value=$2;
      $name =~ s/_/-/g;
      # print "$name=\"$value\"\n";
      $mysqld_variables{$name}= $value;
    }
  }

  # "Convert" innodb flag
  $mysqld_variables{'innodb'}= "ON"
    if ($mysqld_variables{'have_innodb'} eq "YES");

  # Parse version
  my $version_str= $mysqld_variables{'version'};
  if ( $version_str =~ /^([0-9]*)\.([0-9]*)\.([0-9]*)([^\s]*)/ )
  {
    #print "Major: $1 Minor: $2 Build: $3\n";
    $mysql_version_id= $1*10000 + $2*100 + $3;
    #print "mysql_version_id: $mysql_version_id\n";
    mtr_report("MySQL Version $1.$2.$3");
    $mysql_version_extra= $4;
  }
  mtr_error("Could not find version of MySQL") unless $mysql_version_id;
}

sub find_mysqld {

  my ($mysqld_basedir)= $ENV{MTR_BINDIR}|| @_;

  my @mysqld_names= ("mysqld", "mysqld-max-nt", "mysqld-max",
		     "mysqld-nt");

  if ( $opt_debug_server ){
    # Put mysqld-debug first in the list of binaries to look for
    mtr_verbose("Adding mysqld-debug first in list of binaries to look for");
    unshift(@mysqld_names, "mysqld-debug");
  }

  return my_find_bin($bindir,
		     ["sql", "libexec", "sbin", "bin"],
		     [@mysqld_names]);
}


sub executable_setup () {

  $exe_patch='patch' if `patch -v`;

  #
  # Check if libtool is available in this distribution/clone
  # we need it when valgrinding or debugging non installed binary
  # Otherwise valgrind will valgrind the libtool wrapper or bash
  # and gdb will not find the real executable to debug
  #
  if ( -x "../libtool")
  {
    $exe_libtool= "../libtool";
    if ($opt_valgrind or $glob_debugger or $opt_strace)
    {
      mtr_report("Using \"$exe_libtool\" when running valgrind, strace or debugger");
    }
  }

  # Look for the client binaries
  $exe_mysqladmin=     mtr_exe_exists("$path_client_bindir/mysqladmin");
  $exe_mysql=          mtr_exe_exists("$path_client_bindir/mysql");
  $exe_mysql_plugin=   mtr_exe_exists("$path_client_bindir/mysql_plugin");

  $exe_mysql_embedded= mtr_exe_maybe_exists("$basedir/libmysqld/examples/mysql_embedded");

  if ( $ndbcluster_enabled )
  {
    # Look for single threaded NDB
    $exe_ndbd=
      my_find_bin($bindir,
		  ["storage/ndb/src/kernel", "libexec", "sbin", "bin"],
		  "ndbd");

    # Look for multi threaded NDB
    $exe_ndbmtd=
      my_find_bin($bindir,
		  ["storage/ndb/src/kernel", "libexec", "sbin", "bin"],
		  "ndbmtd", NOT_REQUIRED);
    if ($exe_ndbmtd)
    {
      my $mtr_ndbmtd = $ENV{MTR_NDBMTD};
      if ($mtr_ndbmtd)
      {
	mtr_report(" - multi threaded ndbd found, will be used always");
	$exe_ndbd = $exe_ndbmtd;
      }
      else
      {
	mtr_report(" - multi threaded ndbd found, will be ".
		   "used \"round robin\"");
      }
    }

    $exe_ndb_mgmd=
      my_find_bin($bindir,
		  ["storage/ndb/src/mgmsrv", "libexec", "sbin", "bin"],
		  "ndb_mgmd");

    $exe_ndb_mgm=
      my_find_bin($bindir,
                  ["storage/ndb/src/mgmclient", "bin"],
                  "ndb_mgm");

    $exe_ndb_waiter=
      my_find_bin($bindir,
		  ["storage/ndb/tools/", "bin"],
		  "ndb_waiter");

  }

  # Look for mysqltest executable
  if ( $opt_embedded_server )
  {
    $exe_mysqltest=
      mtr_exe_exists("$bindir/libmysqld/examples$opt_vs_config/mysqltest_embedded",
                     "$path_client_bindir/mysqltest_embedded");
  }
  else
  {
    if ( defined $ENV{'MYSQL_TEST'} )
    {
      $exe_mysqltest=$ENV{'MYSQL_TEST'};
      print "===========================================================\n";
      print "WARNING:The mysqltest binary is fetched from $exe_mysqltest\n";
      print "===========================================================\n";
    }
    else
    {
      $exe_mysqltest= mtr_exe_exists("$path_client_bindir/mysqltest");
    }
  }

}


sub client_debug_arg($$) {
  my ($args, $client_name)= @_;

  # Workaround for Bug #50627: drop any debug opt
  return if $client_name =~ /^mysqlbinlog/;

  if ( $opt_debug ) {
    mtr_add_arg($args,
		"--loose-debug=$debug_d:t:A,%s/log/%s.trace",
		$path_vardir_trace, $client_name)
  }
}


sub client_arguments ($;$) {
 my $client_name= shift;
  my $group_suffix= shift;
  my $client_exe= mtr_exe_exists("$path_client_bindir/$client_name");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  if (defined($group_suffix)) {
    mtr_add_arg($args, "--defaults-group-suffix=%s", $group_suffix);
    client_debug_arg($args, "$client_name-$group_suffix");
  }
  else
  {
    client_debug_arg($args, $client_name);
  }
  return mtr_args2str($client_exe, @$args);
}


sub mysqlbinlog_arguments () {
  my $exe= mtr_exe_exists("$path_client_bindir/mysqlbinlog");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--local-load=%s", $opt_tmpdir);
  client_debug_arg($args, "mysqlbinlog");
  return mtr_args2str($exe, @$args);
}


sub mysqlslap_arguments () {
  my $exe= mtr_exe_maybe_exists("$path_client_bindir/mysqlslap");
  if ( $exe eq "" ) {
    # mysqlap was not found

    if (defined $mysql_version_id and $mysql_version_id >= 50100 ) {
      mtr_error("Could not find the mysqlslap binary");
    }
    return ""; # Don't care about mysqlslap
  }

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  client_debug_arg($args, "mysqlslap");
  return mtr_args2str($exe, @$args);
}


sub mysqldump_arguments ($) {
  my($group_suffix) = @_;
  my $exe= mtr_exe_exists("$path_client_bindir/mysqldump");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $group_suffix);
  client_debug_arg($args, "mysqldump-$group_suffix");
  return mtr_args2str($exe, @$args);
}


sub mysql_client_test_arguments(){
  my $exe;
  # mysql_client_test executable may _not_ exist
  if ( $opt_embedded_server ) {
    $exe= mtr_exe_maybe_exists(
            "$bindir/libmysqld/examples$opt_vs_config/mysql_client_test_embedded",
		"$bindir/bin/mysql_client_test_embedded");
  } else {
    $exe= mtr_exe_maybe_exists("$bindir/tests$opt_vs_config/mysql_client_test",
			       "$bindir/bin/mysql_client_test");
  }

  my $args;
  mtr_init_args(\$args);
  if ( $opt_valgrind_mysqltest ) {
    valgrind_arguments($args, \$exe);
  }
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--testcase");
  mtr_add_arg($args, "--vardir=$opt_vardir");
  client_debug_arg($args,"mysql_client_test");
  my $ret=mtr_args2str($exe, @$args);
  return $ret;
}

sub tool_arguments ($$) {
  my($sedir, $tool_name) = @_;
  my $exe= my_find_bin($bindir,
		       [$sedir, "bin"],
		       $tool_name);

  my $args;
  mtr_init_args(\$args);
  client_debug_arg($args, $tool_name);
  return mtr_args2str($exe, @$args);
}

# This is not used to actually start a mysqld server, just to allow test
# scripts to run the mysqld binary to test invalid server startup options.
sub mysqld_client_arguments () {
  my $default_mysqld= default_mysqld();
  my $exe = find_mysqld($bindir);
  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--basedir=%s", $basedir);
  mtr_add_arg($args, "--character-sets-dir=%s", $default_mysqld->value("character-sets-dir"));
  mtr_add_arg($args, "--language=%s", $default_mysqld->value("language"));
  return mtr_args2str($exe, @$args);
}


sub have_maria_support () {
  my $maria_var= $mysqld_variables{'aria-recover'};
  return defined $maria_var;
}

#
# Set environment to be used by childs of this process for
# things that are constant during the whole lifetime of mysql-test-run
#

sub find_plugin($$)
{
  my ($plugin, $location)  = @_;
  my $plugin_filename;

  if (IS_WINDOWS)
  {
     $plugin_filename = $plugin.".dll"; 
  }
  else 
  {
     $plugin_filename = $plugin.".so";
  }

  my $lib_plugin=
    mtr_file_exists(vs_config_dirs($location,$plugin_filename),
                    "$basedir/lib/plugin/".$plugin_filename,
                    "$basedir/lib64/plugin/".$plugin_filename,
                    "$basedir/$location/.libs/".$plugin_filename,
                    "$basedir/lib/mysql/plugin/".$plugin_filename,
                    "$basedir/lib64/mysql/plugin/".$plugin_filename,
                    );
  return $lib_plugin;
}

sub environment_setup {

  umask(022);

  my @ld_library_paths;

  if ($path_client_libdir)
  {
    # Use the --client-libdir passed on commandline
    push(@ld_library_paths, "$path_client_libdir");
  }
  else
  {
    # Setup LD_LIBRARY_PATH so the libraries from this distro/clone
    # are used in favor of the system installed ones
    if ( $source_dist )
    {
      push(@ld_library_paths, "$basedir/libmysql/.libs/",
	   "$basedir/libmysql_r/.libs/",
	   "$basedir/zlib/.libs/");
      if ($^O eq "darwin")
      {
        # it is MAC OS and we have to add dynamic libraries paths
        push @ld_library_paths, grep {<$_/*.dylib>} 
          (<$bindir/storage/*/.libs/>,<$bindir/plugin/*/.libs/>,
          <$bindir/plugin/*/*/.libs/>,<$bindir/storage/*/*/.libs>);
      }
    }
    else
    {
      push(@ld_library_paths, "$basedir/lib", "$basedir/lib/mysql");
    }
  }

  # --------------------------------------------------------------------------
  # Add the path where libndbclient can be found
  # --------------------------------------------------------------------------
  if ( $ndbcluster_enabled )
  {
    push(@ld_library_paths,  "$basedir/storage/ndb/src/.libs");
  }

  # --------------------------------------------------------------------------
  # Valgrind need to be run with debug libraries otherwise it's almost
  # impossible to add correct supressions, that means if "/usr/lib/debug"
  # is available, it should be added to
  # LD_LIBRARY_PATH
  #
  # But pthread is broken in libc6-dbg on Debian <= 3.1 (see Debian
  # bug 399035, http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=399035),
  # so don't change LD_LIBRARY_PATH on that platform.
  # --------------------------------------------------------------------------
  my $debug_libraries_path= "/usr/lib/debug";
  my $deb_version;
  if (  $opt_valgrind and -d $debug_libraries_path and
        (! -e '/etc/debian_version' or
	 ($deb_version=
	    mtr_grab_file('/etc/debian_version')) !~ /^[0-9]+\.[0-9]$/ or
         $deb_version > 3.1 ) )
  {
    push(@ld_library_paths, $debug_libraries_path);
  }

  $ENV{'LD_LIBRARY_PATH'}= join(":", @ld_library_paths,
				$ENV{'LD_LIBRARY_PATH'} ?
				split(':', $ENV{'LD_LIBRARY_PATH'}) : ());
  mtr_debug("LD_LIBRARY_PATH: $ENV{'LD_LIBRARY_PATH'}");

  $ENV{'DYLD_LIBRARY_PATH'}= join(":", @ld_library_paths,
				  $ENV{'DYLD_LIBRARY_PATH'} ?
				  split(':', $ENV{'DYLD_LIBRARY_PATH'}) : ());
  mtr_debug("DYLD_LIBRARY_PATH: $ENV{'DYLD_LIBRARY_PATH'}");

  # The environment variable used for shared libs on AIX
  $ENV{'SHLIB_PATH'}= join(":", @ld_library_paths,
                           $ENV{'SHLIB_PATH'} ?
                           split(':', $ENV{'SHLIB_PATH'}) : ());
  mtr_debug("SHLIB_PATH: $ENV{'SHLIB_PATH'}");

  # The environment variable used for shared libs on hp-ux
  $ENV{'LIBPATH'}= join(":", @ld_library_paths,
                        $ENV{'LIBPATH'} ?
                        split(':', $ENV{'LIBPATH'}) : ());
  mtr_debug("LIBPATH: $ENV{'LIBPATH'}");

  $ENV{'UMASK'}=              "0660"; # The octal *string*
  $ENV{'UMASK_DIR'}=          "0770"; # The octal *string*

  #
  # MySQL tests can produce output in various character sets
  # (especially, ctype_xxx.test). To avoid confusing Perl
  # with output which is incompatible with the current locale
  # settings, we reset the current values of LC_ALL and LC_CTYPE to "C".
  # For details, please see
  # Bug#27636 tests fails if LC_* variables set to *_*.UTF-8
  #
  $ENV{'LC_ALL'}=             "C";
  $ENV{'LC_CTYPE'}=           "C";

  $ENV{'LC_COLLATE'}=         "C";
  $ENV{'USE_RUNNING_SERVER'}= using_extern();
  $ENV{'MYSQL_TEST_DIR'}=     $glob_mysql_test_dir;
  $ENV{'DEFAULT_MASTER_PORT'}= $mysqld_variables{'port'};
  $ENV{'MYSQL_TMP_DIR'}=      $opt_tmpdir;
  $ENV{'MYSQLTEST_VARDIR'}=   $opt_vardir;
  $ENV{'MYSQL_BINDIR'}=       $bindir;
  $ENV{'MYSQL_SHAREDIR'}=     $path_language;
  $ENV{'MYSQL_CHARSETSDIR'}=  $path_charsetsdir;
  
  if (IS_WINDOWS)
  {
    $ENV{'SECURE_LOAD_PATH'}= $glob_mysql_test_dir."\\std_data";
  }
  else
  {
    $ENV{'SECURE_LOAD_PATH'}= $glob_mysql_test_dir."/std_data";
  }

  #
  # Some stupid^H^H^H^H^H^Hignorant network providers set up "wildcard DNS"
  # servers that return some given web server address for any lookup of a
  # non-existent host name. This confuses test cases that want to test the
  # behaviour when connecting to a non-existing host, so we need to be able
  # to disable those tests when DNS is broken.
  #
  $ENV{HAVE_BROKEN_DNS}= defined(gethostbyname('invalid_hostname'));

  # ----------------------------------------------------
  # Setup env for NDB
  # ----------------------------------------------------
  if ( $ndbcluster_enabled )
  {
    $ENV{'NDB_MGM'}=
      my_find_bin($bindir,
		  ["storage/ndb/src/mgmclient", "bin"],
		  "ndb_mgm");

    $ENV{'NDB_TOOLS_DIR'}=
      my_find_dir($bindir,
		  ["storage/ndb/tools", "bin"]);

    $ENV{'NDB_EXAMPLES_DIR'}=
      my_find_dir($basedir,
		  ["storage/ndb/ndbapi-examples", "bin"]);

    $ENV{'NDB_EXAMPLES_BINARY'}=
      my_find_bin($bindir,
		  ["storage/ndb/ndbapi-examples/ndbapi_simple", "bin"],
		  "ndbapi_simple", NOT_REQUIRED);

    my $path_ndb_testrun_log= "$opt_vardir/log/ndb_testrun.log";
    $ENV{'NDB_TOOLS_OUTPUT'}=         $path_ndb_testrun_log;
    $ENV{'NDB_EXAMPLES_OUTPUT'}=      $path_ndb_testrun_log;
  }

  # ----------------------------------------------------
  # mysql clients
  # ----------------------------------------------------
  $ENV{'MYSQL_CHECK'}=              client_arguments("mysqlcheck");
  $ENV{'MYSQL_DUMP'}=               mysqldump_arguments(".1");
  $ENV{'MYSQL_DUMP_SLAVE'}=         mysqldump_arguments(".2");
  $ENV{'MYSQL_SLAP'}=               mysqlslap_arguments();
  $ENV{'MYSQL_IMPORT'}=             client_arguments("mysqlimport");
  $ENV{'MYSQL_SHOW'}=               client_arguments("mysqlshow");
  $ENV{'MYSQL_BINLOG'}=             mysqlbinlog_arguments();
  $ENV{'MYSQL'}=                    client_arguments("mysql");
  $ENV{'MYSQL_SLAVE'}=              client_arguments("mysql", ".2");
  $ENV{'MYSQL_UPGRADE'}=            client_arguments("mysql_upgrade");
  $ENV{'MYSQLADMIN'}=               client_arguments("mysqladmin");
  $ENV{'MYSQL_CLIENT_TEST'}=        mysql_client_test_arguments();
  $ENV{'EXE_MYSQL'}=                $exe_mysql;
  $ENV{'MYSQL_PLUGIN'}=             $exe_mysql_plugin;
  $ENV{'MYSQL_EMBEDDED'}=           $exe_mysql_embedded;

  my $exe_mysqld= find_mysqld($basedir);
  $ENV{'MYSQLD'}= $exe_mysqld;
  my $extra_opts= join (" ", @opt_extra_mysqld_opt);
  $ENV{'MYSQLD_CMD'}= "$exe_mysqld --defaults-group-suffix=.1 ".
    "--defaults-file=$path_config_file $extra_opts";

  # ----------------------------------------------------
  # bug25714 executable may _not_ exist in
  # some versions, test using it should be skipped
  # ----------------------------------------------------
  my $exe_bug25714=
      mtr_exe_maybe_exists("$bindir/tests$opt_vs_config/bug25714");
  $ENV{'MYSQL_BUG25714'}=  native_path($exe_bug25714);

  # ----------------------------------------------------
  # mysql_fix_privilege_tables.sql
  # ----------------------------------------------------
  my $file_mysql_fix_privilege_tables=
    mtr_file_exists("$bindir/scripts/mysql_fix_privilege_tables.sql",
		    "$bindir/share/mysql_fix_privilege_tables.sql",
		    "$bindir/share/mariadb/mysql_fix_privilege_tables.sql",
		    "$bindir/share/mysql/mysql_fix_privilege_tables.sql");
  $ENV{'MYSQL_FIX_PRIVILEGE_TABLES'}=  $file_mysql_fix_privilege_tables;

  # ----------------------------------------------------
  # my_print_defaults
  # ----------------------------------------------------
  my $exe_my_print_defaults=
    mtr_exe_exists("$bindir/extra$opt_vs_config/my_print_defaults",
		   "$path_client_bindir/my_print_defaults");
  $ENV{'MYSQL_MY_PRINT_DEFAULTS'}= native_path($exe_my_print_defaults);

  # ----------------------------------------------------
  # myisam tools
  # ----------------------------------------------------
  $ENV{'MYISAMLOG'}= tool_arguments("storage/myisam", "myisamlog", );
  $ENV{'MYISAMCHK'}= tool_arguments("storage/myisam", "myisamchk");
  $ENV{'MYISAMPACK'}= tool_arguments("storage/myisam", "myisampack");
  $ENV{'MYISAM_FTDUMP'}= tool_arguments("storage/myisam", "myisam_ftdump");

  # ----------------------------------------------------
  # aria tools
  # ----------------------------------------------------
  if (have_maria_support())
  {
    $ENV{'MARIA_CHK'}= tool_arguments("storage/maria", "aria_chk");
    $ENV{'MARIA_PACK'}= tool_arguments("storage/maria", "aria_pack");
  }

  # ----------------------------------------------------
  # mysqlhotcopy
  # ----------------------------------------------------
  my $mysqlhotcopy=
    mtr_pl_maybe_exists("$bindir/scripts/mysqlhotcopy") ||
    mtr_pl_maybe_exists("$path_client_bindir/mysqlhotcopy");
  if ($mysqlhotcopy)
  {
    $ENV{'MYSQLHOTCOPY'}= $mysqlhotcopy;
  }

  # ----------------------------------------------------
  # perror
  # ----------------------------------------------------
  my $exe_perror= mtr_exe_exists("$bindir/extra$opt_vs_config/perror",
				 "$path_client_bindir/perror");
  $ENV{'MY_PERROR'}= native_path($exe_perror);

  # ----------------------------------------------------
  # mysql_tzinfo_to_sql
  # ----------------------------------------------------
  my $exe_mysql_tzinfo_to_sql= mtr_exe_exists("$basedir/sql$opt_vs_config/mysql_tzinfo_to_sql",
                                 "$path_client_bindir/mysql_tzinfo_to_sql",
                                 "$bindir/sql$opt_vs_config/mysql_tzinfo_to_sql");
  $ENV{'MYSQL_TZINFO_TO_SQL'}= native_path($exe_mysql_tzinfo_to_sql);

  # ----------------------------------------------------
  # replace
  # ----------------------------------------------------
  my $exe_replace= mtr_exe_exists(vs_config_dirs('extra', 'replace'),
                                 "$basedir/extra/replace",
                                 "$bindir/extra$opt_vs_config/replace",
                                 "$path_client_bindir/replace");
  $ENV{'REPLACE'}= native_path($exe_replace);

  # Create an environment variable to make it possible
  # to detect that valgrind is being used from test cases
  $ENV{'VALGRIND_TEST'}= $opt_valgrind;

  # Add dir of this perl to aid mysqltest in finding perl
  my $perldir= dirname($^X);
  my $pathsep= ":";
  $pathsep= ";" if IS_WINDOWS && ! IS_CYGWIN;
  $ENV{'PATH'}= "$ENV{'PATH'}".$pathsep.$perldir;
}


sub remove_vardir_subs() {
  foreach my $sdir ( glob("$opt_vardir/*") ) {
    mtr_verbose("Removing subdir $sdir");
    rmtree($sdir);
  }
}

#
# Remove var and any directories in var/ created by previous
# tests
#
sub remove_stale_vardir () {

  mtr_report("Removing old var directory...");

  # Safety!
  mtr_error("No, don't remove the vardir when running with --extern")
    if using_extern();

  mtr_verbose("opt_vardir: $opt_vardir");
  if ( $opt_vardir eq $default_vardir )
  {
    #
    # Running with "var" in mysql-test dir
    #
    if ( -l $opt_vardir)
    {
      # var is a symlink

      if ( $opt_mem )
      {
	# Remove the directory which the link points at
	mtr_verbose("Removing " . readlink($opt_vardir));
	rmtree(readlink($opt_vardir));

	# Remove the "var" symlink
	mtr_verbose("unlink($opt_vardir)");
	unlink($opt_vardir);
      }
      else
      {
	# Some users creates a soft link in mysql-test/var to another area
	# - allow it, but remove all files in it

	mtr_report(" - WARNING: Using the 'mysql-test/var' symlink");

	# Make sure the directory where it points exist
        if (! -d readlink($opt_vardir))
        {
          mtr_report("The destination for symlink $opt_vardir does not exist; Removing it and creating a new var directory");
          unlink($opt_vardir);
        }
	remove_vardir_subs();
      }
    }
    else
    {
      # Remove the entire "var" dir
      mtr_verbose("Removing $opt_vardir/");
      rmtree("$opt_vardir/");
    }

    if ( $opt_mem )
    {
      # A symlink from var/ to $opt_mem will be set up
      # remove the $opt_mem dir to assure the symlink
      # won't point at an old directory
      mtr_verbose("Removing $opt_mem");
      rmtree($opt_mem);
    }

  }
  else
  {
    #
    # Running with "var" in some other place
    #

    # Don't remove the var/ dir in mysql-test dir as it may be in
    # use by another mysql-test-run run with --vardir
    # mtr_verbose("Removing $default_vardir");
    # rmtree($default_vardir);

    # Remove the "var" dir
    mtr_verbose("Removing $opt_vardir/");
    rmtree("$opt_vardir/");
  }
  # Remove the "tmp" dir
  mtr_verbose("Removing $opt_tmpdir/");
  rmtree("$opt_tmpdir/");
}

sub set_plugin_var($) {
  local $_ = $_[0];
  s/\.\w+$//;
  $ENV{"\U${_}_SO"} = $_[0];
}

#
# Create var and the directories needed in var
#
sub setup_vardir() {
  mtr_report("Creating var directory '$opt_vardir'...");

  if ( $opt_vardir eq $default_vardir )
  {
    #
    # Running with "var" in mysql-test dir
    #
    if ( -l $opt_vardir )
    {
      #  it's a symlink

      # Make sure the directory where it points exist
      if (! -d readlink($opt_vardir))
      {
        mtr_report("The destination for symlink $opt_vardir does not exist; Removing it and creating a new var directory");
        unlink($opt_vardir);
      }
    }
    elsif ( $opt_mem )
    {
      # Runinng with "var" as a link to some "memory" location, normally tmpfs
      mtr_verbose("Creating $opt_mem");
      mkpath($opt_mem);

      mtr_report(" - symlinking 'var' to '$opt_mem'");
      symlink($opt_mem, $opt_vardir);
    }
  }

  if ( ! -d $opt_vardir )
  {
    mtr_verbose("Creating $opt_vardir");
    mkpath($opt_vardir);
  }

  # Ensure a proper error message if vardir couldn't be created
  unless ( -d $opt_vardir and -w $opt_vardir )
  {
    mtr_error("Writable 'var' directory is needed, use the " .
	      "'--vardir=<path>' option");
  }

  mkpath("$opt_vardir/log");
  mkpath("$opt_vardir/run");

  # Create var/tmp and tmp - they might be different
  mkpath("$opt_vardir/tmp");
  mkpath($opt_tmpdir) if ($opt_tmpdir ne "$opt_vardir/tmp");

  # On some operating systems, there is a limit to the length of a
  # UNIX domain socket's path far below PATH_MAX.
  # Don't allow that to happen
  if (check_socket_path_length("$opt_tmpdir/testsocket.sock")){
    mtr_error("Socket path '$opt_tmpdir' too long, it would be ",
	      "truncated and thus not possible to use for connection to ",
	      "MySQL Server. Set a shorter with --tmpdir=<path> option");
  }

  # copy all files from std_data into var/std_data
  # and make them world readable
  copytree("$glob_mysql_test_dir/std_data", "$opt_vardir/std_data", "0022");

  # create a plugin dir and copy plugins into it
  if ($source_dist)
  {
    $plugindir="$opt_vardir/plugins";
    mkpath($plugindir);
    if (IS_WINDOWS && !$opt_embedded_server)
    {
      for (<$bindir/storage/*$opt_vs_config/*.dll>,
           <$bindir/plugin/*$opt_vs_config/*.dll>,
           <$bindir/sql$opt_vs_config/*.dll>)
      {
        my $pname=basename($_);
        copy rel2abs($_), "$plugindir/$pname";
        set_plugin_var($pname);
      }
    }
    else
    {
      for (<../storage/*/.libs/*.so>,
           <../plugin/*/.libs/*.so>,
           <../plugin/*/*/.libs/*.so>,
           <../sql/.libs/*.so>,
           <$bindir/storage/*/*.so>,
           <$bindir/plugin/*/*.so>,
           <$bindir/sql/*.so>)
      {
        my $pname=basename($_);
        symlink rel2abs($_), "$plugindir/$pname";
        set_plugin_var($pname);
      }
    }
  }
  else
  {
    $plugindir= $mysqld_variables{'plugin-dir'} || '.';
    # hm, what paths work for debs and for rpms ?
    for (<$bindir/lib64/mysql/plugin/*.so>,
         <$bindir/lib/mysql/plugin/*.so>,
         <$bindir/lib/plugin/*.so>,             # bintar
         <$bindir/lib/plugin/*.dll>)
    {
      my $pname=basename($_);
      set_plugin_var($pname);
    }
  }

  # Remove old log files
  foreach my $name (glob("r/*.progress r/*.log r/*.warnings"))
  {
    unlink($name);
  }
}


#
# Check if running as root
# i.e a file can be read regardless what mode we set it to
#
sub  check_running_as_root () {
  my $test_file= "$opt_vardir/test_running_as_root.txt";
  mtr_tofile($test_file, "MySQL");
  chmod(oct("0000"), $test_file);

  my $result="";
  if (open(FILE,"<",$test_file))
  {
    $result= join('', <FILE>);
    close FILE;
  }

  # Some filesystems( for example CIFS) allows reading a file
  # although mode was set to 0000, but in that case a stat on
  # the file will not return 0000
  my $file_mode= (stat($test_file))[2] & 07777;

  mtr_verbose("result: $result, file_mode: $file_mode");
  if ($result eq "MySQL" && $file_mode == 0)
  {
    mtr_warning("running this script as _root_ will cause some " .
                "tests to be skipped");
    $ENV{'MYSQL_TEST_ROOT'}= "1";
  }

  chmod(oct("0755"), $test_file);
  unlink($test_file);
}


sub check_ssl_support {
  if ($opt_skip_ssl)
  {
    mtr_report(" - skipping SSL");
    $opt_ssl_supported= 0;
    $opt_ssl= 0;
    return;
  }

  if ( ! $mysqld_variables{'ssl'} )
  {
    if ( $opt_ssl)
    {
      mtr_error("Couldn't find support for SSL");
      return;
    }
    mtr_report(" - skipping SSL, mysqld not compiled with SSL");
    $opt_ssl_supported= 0;
    $opt_ssl= 0;
    return;
  }
  mtr_report(" - SSL connections supported");
  $opt_ssl_supported= 1;
}

sub check_debug_support {
  if (defined $mysqld_variables{'debug-dbug'})
  {
    mtr_report(" - binaries are debug compiled");
  }
  elsif ($opt_debug_server)
  {
    mtr_error("Can't use --debug[-server], binary does not support it");
  }
}


#
# Helper function to find the correct value for the opt_vs_config
# if it was not set explicitly.
#
# the configuration with the most recent build dir in sql/ is selected.
#
# note: looking for all BuildLog.htm files everywhere in the tree with the
# help of File::Find would be possibly more precise, but it is also
# many times slower. Thus we are only looking at the server, client
# executables, and plugins - that is, something that can affect the test suite
#
sub fix_vs_config_dir () {
  return $opt_vs_config="" unless IS_WINDOWS;
  return $opt_vs_config="/$opt_vs_config" if $opt_vs_config;

  my $modified = 1e30;
  $opt_vs_config="";


  for (<$bindir/sql/*/mysqld.exe>) { #/
    if (-M $_ < $modified)
    {
      $modified = -M _;
      $opt_vs_config = basename(dirname($_));
    }
  }

  mtr_report("VS config: $opt_vs_config");
  $opt_vs_config="/$opt_vs_config" if $opt_vs_config;
}


#
# Helper function to handle configuration-based subdirectories which Visual
# Studio uses for storing binaries.  If opt_vs_config is set, this returns
# a path based on that setting; if not, it returns paths for the default
# /release/ and /debug/ subdirectories.
#
# $exe can be undefined, if the directory itself will be used
#
sub vs_config_dirs ($$) {
  my ($path_part, $exe) = @_;

  $exe = "" if not defined $exe;

  # Don't look in these dirs when not on windows
  return () unless IS_WINDOWS;

  if ($opt_vs_config)
  {
    return ("$basedir/$path_part/$opt_vs_config/$exe");
  }

  return ("$basedir/$path_part/release/$exe",
          "$basedir/$path_part/relwithdebinfo/$exe",
          "$basedir/$path_part/debug/$exe");
}


sub check_ndbcluster_support {

  my $ndbcluster_supported = 0;
  if ($mysqld_variables{'ndb-connectstring'})
  {
    $ndbcluster_supported = 1;
  }

  if ($opt_skip_ndbcluster && $opt_include_ndbcluster)
  {
    # User is ambivalent. Theoretically the arg which was
    # given last on command line should win, but that order is
    # unknown at this time.
    mtr_error("Ambigous command, both --include-ndbcluster " .
	      " and --skip-ndbcluster was specified");
  }

  # Check if this is MySQL Cluster, ie. mysql version string ends
  # with -ndb-Y.Y.Y[-status]
  if ( defined $mysql_version_extra &&
       $mysql_version_extra =~ /-ndb-([0-9]*)\.([0-9]*)\.([0-9]*)/ )
  {
    # MySQL Cluster tree
    mtr_report(" - MySQL Cluster detected");

    if ($opt_skip_ndbcluster)
    {
      mtr_report(" - skipping ndbcluster(--skip-ndbcluster)");
      return;
    }

    if (!$ndbcluster_supported)
    {
      # MySQL Cluster tree, but mysqld was not compiled with
      # ndbcluster -> fail unless --skip-ndbcluster was used
      mtr_error("This is MySQL Cluster but mysqld does not " .
		"support ndbcluster. Use --skip-ndbcluster to " .
		"force mtr to run without it.");
    }

    # mysqld was compiled with ndbcluster -> auto enable
  }
  else
  {
    # Not a MySQL Cluster tree
    if (!$ndbcluster_supported)
    {
      if ($opt_include_ndbcluster)
      {
	mtr_error("Could not detect ndbcluster support ".
		  "requested with --include-ndbcluster");
      }

      # Silently skip, mysqld was compiled without ndbcluster
      # which is the default case
      return;
    }

    if ($opt_skip_ndbcluster)
    {
      # Compiled with ndbcluster but ndbcluster skipped
      mtr_report(" - skipping ndbcluster(--skip-ndbcluster)");
      return;
    }


    # Not a MySQL Cluster tree, enable ndbcluster
    # if --include-ndbcluster was used
    if ($opt_include_ndbcluster)
    {
      # enable ndbcluster
    }
    else
    {
      mtr_report(" - skipping ndbcluster(disabled by default)");
      return;
    }
  }

  mtr_report(" - enabling ndbcluster");
  $ndbcluster_enabled= 1;
  # Add MySQL Cluster test suites
  push @DEFAULT_SUITES, qw(ndb ndb_binlog rpl_ndb ndb_rpl ndb_memcache);
  return;
}


sub ndbcluster_wait_started {
  my $cluster= shift;
  my $ndb_waiter_extra_opt= shift;
  my $path_waitlog= join('/', $opt_vardir, $cluster->name(), "ndb_waiter.log");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $cluster->suffix());
  mtr_add_arg($args, "--timeout=%d", $opt_start_timeout);

  if ($ndb_waiter_extra_opt)
  {
    mtr_add_arg($args, "$ndb_waiter_extra_opt");
  }

  # Start the ndb_waiter which will connect to the ndb_mgmd
  # and poll it for state of the ndbd's, will return when
  # all nodes in the cluster is started

  my $res= My::SafeProcess->run
    (
     name          => "ndb_waiter ".$cluster->name(),
     path          => $exe_ndb_waiter,
     args          => \$args,
     output        => $path_waitlog,
     error         => $path_waitlog,
     append        => 1,
    );

  # Check that ndb_mgmd(s) are still alive
  foreach my $ndb_mgmd ( in_cluster($cluster, ndb_mgmds()) )
  {
    my $proc= $ndb_mgmd->{proc};
    if ( ! $proc->wait_one(0) )
    {
      mtr_warning("$proc died");
      return 2;
    }
  }

  # Check that all started ndbd(s) are still alive
  foreach my $ndbd ( in_cluster($cluster, ndbds()) )
  {
    my $proc= $ndbd->{proc};
    next unless defined $proc;
    if ( ! $proc->wait_one(0) )
    {
      mtr_warning("$proc died");
      return 3;
    }
  }

  if ($res)
  {
    mtr_verbose("ndbcluster_wait_started failed");
    return 1;
  }
  return 0;
}


sub ndb_mgmd_wait_started($) {
  my ($cluster)= @_;

  my $retries= 100;
  while ($retries)
  {
    my $result= ndbcluster_wait_started($cluster, "--no-contact");
    if ($result == 0)
    {
      # ndb_mgmd is started
      mtr_verbose("ndb_mgmd is started");
      return 0;
    }
    elsif ($result > 1)
    {
      mtr_warning("Cluster process failed while waiting for start");
      return $result;
    }

    mtr_milli_sleep(100);
    $retries--;
  }

  return 1;
}

sub ndb_mgmd_stop{
  my $ndb_mgmd= shift or die "usage: ndb_mgmd_stop(<ndb_mgmd>)";

  my $host=$ndb_mgmd->value('HostName');
  my $port=$ndb_mgmd->value('PortNumber');
  mtr_verbose("Stopping cluster '$host:$port'");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--ndb-connectstring=%s:%s", $host,$port);
  mtr_add_arg($args, "-e");
  mtr_add_arg($args, "shutdown");

  My::SafeProcess->run
    (
     name          => "ndb_mgm shutdown $host:$port",
     path          => $exe_ndb_mgm,
     args          => \$args,
     output         => "/dev/null",
    );
}

sub ndb_mgmd_start ($$) {
  my ($cluster, $ndb_mgmd)= @_;

  mtr_verbose("ndb_mgmd_start");

  my $dir= $ndb_mgmd->value("DataDir");
  mkpath($dir) unless -d $dir;

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $cluster->suffix());
  mtr_add_arg($args, "--mycnf");
  mtr_add_arg($args, "--nodaemon");

  my $path_ndb_mgmd_log= "$dir/ndb_mgmd.log";

  $ndb_mgmd->{'proc'}= My::SafeProcess->new
    (
     name          => $ndb_mgmd->after('cluster_config.'),
     path          => $exe_ndb_mgmd,
     args          => \$args,
     output        => $path_ndb_mgmd_log,
     error         => $path_ndb_mgmd_log,
     append        => 1,
     verbose       => $opt_verbose,
     shutdown      => sub { ndb_mgmd_stop($ndb_mgmd) },
    );
  mtr_verbose("Started $ndb_mgmd->{proc}");

  # FIXME Should not be needed
  # Unfortunately the cluster nodes will fail to start
  # if ndb_mgmd has not started properly
  if (ndb_mgmd_wait_started($cluster))
  {
    mtr_warning("Failed to wait for start of ndb_mgmd");
    return 1;
  }

  return 0;
}

sub ndbd_stop {
  # Intentionally left empty, ndbd nodes will be shutdown
  # by sending "shutdown" to ndb_mgmd
}

our $exe_ndbmtd_counter= 0;

sub ndbd_start {
  my ($cluster, $ndbd)= @_;

  mtr_verbose("ndbd_start");

  my $dir= $ndbd->value("DataDir");
  mkpath($dir) unless -d $dir;

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $cluster->suffix());
  mtr_add_arg($args, "--nodaemon");

# > 5.0 { 'character-sets-dir' => \&fix_charset_dir },

  my $exe= $exe_ndbd;
  if ($exe_ndbmtd and ($exe_ndbmtd_counter++ % 2) == 0)
  {
    # Use ndbmtd every other time
    $exe= $exe_ndbmtd;
  }

  my $path_ndbd_log= "$dir/ndbd.log";
  my $proc= My::SafeProcess->new
    (
     name          => $ndbd->after('cluster_config.'),
     path          => $exe,
     args          => \$args,
     output        => $path_ndbd_log,
     error         => $path_ndbd_log,
     append        => 1,
     verbose       => $opt_verbose,
     shutdown      => sub { ndbd_stop($ndbd) },
    );
  mtr_verbose("Started $proc");

  $ndbd->{proc}= $proc;

  return;
}


sub ndbcluster_start ($) {
  my ($cluster) = @_;

  mtr_verbose("ndbcluster_start '".$cluster->name()."'");

  foreach my $ndb_mgmd ( in_cluster($cluster, ndb_mgmds()) )
  {
    next if started($ndb_mgmd);
    ndb_mgmd_start($cluster, $ndb_mgmd);
  }

  foreach my $ndbd ( in_cluster($cluster, ndbds()) )
  {
    next if started($ndbd);
    ndbd_start($cluster, $ndbd);
  }

  return 0;
}


sub mysql_server_start($) {
  my ($mysqld, $tinfo) = @_;

  if ( $mysqld->{proc} )
  {
    # Already started

    # Write start of testcase to log file
    mark_log($mysqld->value('#log-error'), $tinfo);

    return;
  }

  my $datadir= $mysqld->value('datadir');
  if (not $opt_start_dirty)
  {

    my @options= ('log-bin', 'relay-log');
    foreach my $option_name ( @options )  {
      next unless $mysqld->option($option_name);

      my $file_name= $mysqld->value($option_name);
      next unless
        defined $file_name and
          -e $file_name;

      mtr_debug(" -removing '$file_name'");
      unlink($file_name) or die ("unable to remove file '$file_name'");
    }

    if (-d $datadir ) {
      mtr_verbose(" - removing '$datadir'");
      rmtree($datadir);
    }
  }

  my $mysqld_basedir= $mysqld->value('basedir');
  if ( $basedir eq $mysqld_basedir )
  {
    if (! $opt_start_dirty)	# If dirty, keep possibly grown system db
    {
      # Copy datadir from installed system db
      for my $path ( "$opt_vardir", "$opt_vardir/..") {
        my $install_db= "$path/install.db";
        copytree($install_db, $datadir)
          if -d $install_db;
      }
      mtr_error("Failed to copy system db to '$datadir'")
        unless -d $datadir;
    }
  }
  else
  {
    mysql_install_db($mysqld); # For versional testing

    mtr_error("Failed to install system db to '$datadir'")
      unless -d $datadir;

  }

  # Create the servers tmpdir
  my $tmpdir= $mysqld->value('tmpdir');
  mkpath($tmpdir) unless -d $tmpdir;

  # Write start of testcase to log file
  mark_log($mysqld->value('#log-error'), $tinfo);

  # Run <tname>-master.sh
  if ($mysqld->option('#!run-master-sh') and
      defined $tinfo->{master_sh} and 
      run_system('/bin/sh ' . $tinfo->{master_sh}) )
  {
    $tinfo->{'comment'}= "Failed to execute '$tinfo->{master_sh}'";
    return 1;
  }

  # Run <tname>-slave.sh
  if ($mysqld->option('#!run-slave-sh') and
      defined $tinfo->{slave_sh} and
      run_system('/bin/sh ' . $tinfo->{slave_sh}))
  {
    $tinfo->{'comment'}= "Failed to execute '$tinfo->{slave_sh}'";
    return 1;
  }

  if (!$opt_embedded_server)
  {
    my $extra_opts= get_extra_opts($mysqld, $tinfo);
    mysqld_start($mysqld,$extra_opts);

    # Save this test case information, so next can examine it
    $mysqld->{'started_tinfo'}= $tinfo;
  }
}

sub mysql_server_wait {
  my ($mysqld) = @_;

  return not sleep_until_file_created($mysqld->value('pid-file'),
                                      $opt_start_timeout,
                                      $mysqld->{'proc'},
                                      $warn_seconds);
}

sub create_config_file_for_extern {
  my %opts=
    (
     socket     => '/tmp/mysqld.sock',
     port       => 3306,
     user       => $opt_user,
     password   => '',
     @_
    );

  mtr_report("Creating my.cnf file for extern server...");
  my $F= IO::File->new($path_config_file, "w")
    or mtr_error("Can't write to $path_config_file: $!");

  print $F "[client]\n";
  while (my ($option, $value)= each( %opts )) {
    print $F "$option= $value\n";
    mtr_report(" $option= $value");
  }

  print $F <<EOF

# binlog reads from [client] and [mysqlbinlog]
[mysqlbinlog]
character-sets-dir= $path_charsetsdir
local-load= $opt_tmpdir

EOF
;

  $F= undef; # Close file
}


#
# Kill processes left from previous runs, normally
# there should be none so make sure to warn
# if there is one
#
sub kill_leftovers ($) {
  my $rundir= shift;
  return unless ( -d $rundir );

  mtr_report("Checking leftover processes...");

  # Scan the "run" directory for process id's to kill
  opendir(RUNDIR, $rundir)
    or mtr_error("kill_leftovers, can't open dir \"$rundir\": $!");
  while ( my $elem= readdir(RUNDIR) )
  {
    # Only read pid from files that end with .pid
    if ( $elem =~ /.*[.]pid$/ )
    {
      my $pidfile= "$rundir/$elem";
      next unless -f $pidfile;
      my $pid= mtr_fromfile($pidfile);
      unlink($pidfile);
      unless ($pid=~ /^(\d+)/){
	# The pid was not a valid number
	mtr_warning("Got invalid pid '$pid' from '$elem'");
	next;
      }
      mtr_report(" - found old pid $pid in '$elem', killing it...");

      my $ret= kill("KILL", $pid);
      if ($ret == 0) {
	mtr_report("   process did not exist!");
	next;
      }

      my $check_counter= 100;
      while ($ret > 0 and $check_counter--) {
	mtr_milli_sleep(100);
	$ret= kill(0, $pid);
      }
      mtr_report($check_counter ? "   ok!" : "   failed!");
    }
    else
    {
      mtr_warning("Found non pid file '$elem' in '$rundir'")
	if -f "$rundir/$elem";
    }
  }
  closedir(RUNDIR);
}

#
# Check that all the ports that are going to
# be used are free
#
sub check_ports_free ($)
{
  my $bthread= shift;
  my $portbase = $bthread * 10 + 10000;
  for ($portbase..$portbase+9){
    if (mtr_ping_port($_)){
      mtr_report(" - 'localhost:$_' was not free");
      return 0; # One port was not free
    }
  }

  return 1; # All ports free
}


sub initialize_servers {

  if ( using_extern() )
  {
    # Running against an already started server, if the specified
    # vardir does not already exist it should be created
    if ( ! -d $opt_vardir )
    {
      setup_vardir();
    }
    else
    {
      mtr_verbose("No need to create '$opt_vardir' it already exists");
    }
  }
  else
  {
    # Kill leftovers from previous run
    # using any pidfiles found in var/run
    kill_leftovers("$opt_vardir/run");

    if ( ! $opt_start_dirty )
    {
      remove_stale_vardir();
      setup_vardir();
    }
  }
}


#
# Remove all newline characters expect after semicolon
#
sub sql_to_bootstrap {
  my ($sql) = @_;
  my @lines= split(/\n/, $sql);
  my $result= "\n";
  my $delimiter= ';';

  foreach my $line (@lines) {

    # Change current delimiter if line starts with "delimiter"
    if ( $line =~ /^delimiter (.*)/ ) {
      my $new= $1;
      # Remove old delimiter from end of new
      $new=~ s/\Q$delimiter\E$//;
      $delimiter = $new;
      mtr_debug("changed delimiter to $delimiter");
      # No need to add the delimiter to result
      next;
    }

    # Add newline if line ends with $delimiter
    # and convert the current delimiter to semicolon
    if ( $line =~ /\Q$delimiter\E$/ ){
      $line =~ s/\Q$delimiter\E$/;/;
      $result.= "$line\n";
      mtr_debug("Added default delimiter");
      next;
    }

    # Remove comments starting with --
    if ( $line =~ /^\s*--/ ) {
      mtr_debug("Discarded $line");
      next;
    }

    # Replace @HOSTNAME with localhost
    $line=~ s/\'\@HOSTNAME\@\'/localhost/;

    # Default, just add the line without newline
    # but with a space as separator
    $result.= "$line ";

  }
  return $result;
}


sub default_mysqld {
  # Generate new config file from template
  environment_setup();
  my $config= My::ConfigFactory->new_config
    ( {
       basedir         => $basedir,
       testdir         => $glob_mysql_test_dir,
       template_path   => "include/default_my.cnf",
       vardir          => $opt_vardir,
       tmpdir          => $opt_tmpdir,
       baseport        => 0,
       user            => $opt_user,
       password        => '',
      }
    );

  my $mysqld= $config->group('mysqld.1')
    or mtr_error("Couldn't find mysqld.1 in default config");
  return $mysqld;
}


sub mysql_install_db {
  my ($mysqld, $datadir)= @_;

  my $install_datadir= $datadir || $mysqld->value('datadir');
  my $install_basedir= $mysqld->value('basedir');
  my $install_lang= $mysqld->value('lc-messages-dir');
  my $install_chsdir= $mysqld->value('character-sets-dir');

  mtr_report("Installing system database...");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--bootstrap");
  mtr_add_arg($args, "--basedir=%s", $install_basedir);
  mtr_add_arg($args, "--datadir=%s", $install_datadir);
  mtr_add_arg($args, "--default-storage-engine=myisam");
  mtr_add_arg($args, "--skip-$_") for @optional_plugins;
  mtr_add_arg($args, "--disable-sync-frm");
  mtr_add_arg($args, "--tmpdir=%s", "$opt_vardir/tmp/");
  mtr_add_arg($args, "--core-file");

  if ( $opt_debug )
  {
    mtr_add_arg($args, "--debug-dbug=$debug_d:t:i:A,%s/log/bootstrap.trace",
		$path_vardir_trace);
  }

  mtr_add_arg($args, "--lc-messages-dir=%s", $install_lang);
  mtr_add_arg($args, "--character-sets-dir=%s", $install_chsdir);

  # InnoDB arguments that affect file location and sizes may
  # need to be given to the bootstrap process as well as the
  # server process.
  foreach my $extra_opt ( @opt_extra_mysqld_opt ) {
    if ($extra_opt =~ /--innodb/) {
      mtr_add_arg($args, $extra_opt);
    }
  }

  # If DISABLE_GRANT_OPTIONS is defined when the server is compiled (e.g.,
  # configure --disable-grant-options), mysqld will not recognize the
  # --bootstrap or --skip-grant-tables options.  The user can set
  # MYSQLD_BOOTSTRAP to the full path to a mysqld which does accept
  # --bootstrap, to accommodate this.
  my $exe_mysqld_bootstrap =
    $ENV{'MYSQLD_BOOTSTRAP'} || find_mysqld($install_basedir);

  # ----------------------------------------------------------------------
  # export MYSQLD_BOOTSTRAP_CMD variable containing <path>/mysqld <args>
  # ----------------------------------------------------------------------
  $ENV{'MYSQLD_BOOTSTRAP_CMD'}= "$exe_mysqld_bootstrap " . join(" ", @$args);



  # ----------------------------------------------------------------------
  # Create the bootstrap.sql file
  # ----------------------------------------------------------------------
  my $bootstrap_sql_file= "$opt_vardir/tmp/bootstrap.sql";

  if ($opt_boot_gdb) {
    gdb_arguments(\$args, \$exe_mysqld_bootstrap, $mysqld->name(),
		  $bootstrap_sql_file);
  }
  if ($opt_boot_dbx) {
    dbx_arguments(\$args, \$exe_mysqld_bootstrap, $mysqld->name(),
		  $bootstrap_sql_file);
  }
  if ($opt_boot_ddd) {
    ddd_arguments(\$args, \$exe_mysqld_bootstrap, $mysqld->name(),
		  $bootstrap_sql_file);
  }

  my $path_sql= my_find_file($install_basedir,
			     ["mysql", "sql/share", "share/mariadb",
			      "share/mysql", "share", "scripts"],
			     "mysql_system_tables.sql",
			     NOT_REQUIRED);

  if (-f $path_sql )
  {
    my $sql_dir= dirname($path_sql);
    # Use the mysql database for system tables
    mtr_tofile($bootstrap_sql_file, "use mysql\n");

    # Add the offical mysql system tables
    # for a production system
    mtr_appendfile_to_file("$sql_dir/mysql_system_tables.sql",
			   $bootstrap_sql_file);

    # Add the performance tables
    # for a production system
    mtr_appendfile_to_file("$sql_dir/mysql_performance_tables.sql",
                          $bootstrap_sql_file);

    # Add the mysql system tables initial data
    # for a production system
    mtr_appendfile_to_file("$sql_dir/mysql_system_tables_data.sql",
			   $bootstrap_sql_file);

    # Add test data for timezone - this is just a subset, on a real
    # system these tables will be populated either by mysql_tzinfo_to_sql
    # or by downloading the timezone table package from our website
    mtr_appendfile_to_file("$sql_dir/mysql_test_data_timezone.sql",
			   $bootstrap_sql_file);

    # Fill help tables, just an empty file when running from bk repo
    # but will be replaced by a real fill_help_tables.sql when
    # building the source dist
    mtr_appendfile_to_file("$sql_dir/fill_help_tables.sql",
			   $bootstrap_sql_file);

  }
  else
  {
    # Install db from init_db.sql that exist in early 5.1 and 5.0
    # versions of MySQL
    my $init_file= "$install_basedir/mysql-test/lib/init_db.sql";
    mtr_report(" - from '$init_file'");
    my $text= mtr_grab_file($init_file) or
      mtr_error("Can't open '$init_file': $!");

    mtr_tofile($bootstrap_sql_file,
	       sql_to_bootstrap($text));
  }

  # Remove anonymous users
  mtr_tofile($bootstrap_sql_file,
	     "DELETE FROM mysql.user where user= '';\n");

  # Create mtr database
  mtr_tofile($bootstrap_sql_file,
	     "CREATE DATABASE mtr;\n");

  # Add help tables and data for warning detection and supression
  mtr_tofile($bootstrap_sql_file,
             sql_to_bootstrap(mtr_grab_file("include/mtr_warnings.sql")));

  # Add procedures for checking server is restored after testcase
  mtr_tofile($bootstrap_sql_file,
             sql_to_bootstrap(mtr_grab_file("include/mtr_check.sql")));

  # Log bootstrap command
  my $path_bootstrap_log= "$opt_vardir/log/bootstrap.log";
  mtr_tofile($path_bootstrap_log,
	     "$exe_mysqld_bootstrap " . join(" ", @$args) . "\n");

  # Create directories mysql and test
  mkpath("$install_datadir/mysql");
  mkpath("$install_datadir/test");

  if ( My::SafeProcess->run
       (
	name          => "bootstrap",
	path          => $exe_mysqld_bootstrap,
	args          => \$args,
	input         => $bootstrap_sql_file,
	output        => $path_bootstrap_log,
	error         => $path_bootstrap_log,
	append        => 1,
	verbose       => $opt_verbose,
       ) != 0)
  {
    mtr_error("Error executing mysqld --bootstrap\n" .
              "Could not install system database from $bootstrap_sql_file\n" .
	      "see $path_bootstrap_log for errors");
  }
}


sub run_testcase_check_skip_test($)
{
  my ($tinfo)= @_;

  # ----------------------------------------------------------------------
  # Skip some tests silently
  # ----------------------------------------------------------------------

  my $start_from= $mtr_cases::start_from;
  if ( $start_from )
  {
    if ($tinfo->{'name'} eq $start_from ||
        $tinfo->{'shortname'} eq $start_from)
    {
      ## Found parting test. Run this test and all tests after this one
      $mtr_cases::start_from= "";
    }
    else
    {
      $tinfo->{'result'}= 'MTR_RES_SKIPPED';
      return 1;
    }
  }

  # ----------------------------------------------------------------------
  # If marked to skip, just print out and return.
  # Note that a test case not marked as 'skip' can still be
  # skipped later, because of the test case itself in cooperation
  # with the mysqltest program tells us so.
  # ----------------------------------------------------------------------

  if ( $tinfo->{'skip'} )
  {
    mtr_report_test_skipped($tinfo) unless $start_only;
    return 1;
  }

  return 0;
}


sub run_query {
  my ($tinfo, $mysqld, $query)= @_;

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));

  mtr_add_arg($args, "-e %s", $query);

  my $res= My::SafeProcess->run
    (
     name          => "run_query -> ".$mysqld->name(),
     path          => $exe_mysql,
     args          => \$args,
     output        => '/dev/null',
     error         => '/dev/null'
    );

  return $res
}


sub do_before_run_mysqltest($)
{
  my $tinfo= shift;
  my $resfile= $tinfo->{result_file};
  return unless defined $resfile;

  # Remove old files produced by mysqltest
  die "unsupported result file name $resfile, stoping" unless
         $resfile =~ /^(.*?)((?:,\w+)*)\.(rdiff|result|result~)$/;
  my ($base_file, $suites, $ext)= ($1, $2, $3);
  # if the result file is a diff, make a proper result file
  if ($ext eq 'rdiff') {
    my $base_result = $tinfo->{base_result};
    my $resdir= dirname($resfile);
    # we'll use a separate extension for generated result files
    # to be able to distinguish them from manually created
    # version-controlled results, and to ignore them in bzr.
    my $dest = "$base_file$suites.result~";
    my @cmd = ($exe_patch, qw/--binary -r - -f -s -o/,
               $dest, $base_result, $resfile);
    if (-w $resdir) {
      # don't rebuild a file if it's up to date
      unless (-e $dest and -M $dest < -M $resfile
                       and -M $dest < -M $base_result) {
        run_system(@cmd);
      }
    } else {
      $cmd[-3] = $dest = $opt_tmpdir . '/' . basename($dest);
      run_system(@cmd);
    }
    $tinfo->{result_file} = $dest;
  }

  unlink("$base_file.reject");
  unlink("$base_file.progress");
  unlink("$base_file.log");
  unlink("$base_file.warnings");
}


#
# Check all server for sideffects
#
# RETURN VALUE
#  0 ok
#  1 Check failed
#  >1 Fatal errro

sub check_testcase($$)
{
  my ($tinfo, $mode)= @_;
  my $tname= $tinfo->{name};

  # Start the mysqltest processes in parallel to save time
  # also makes it possible to wait for any process to exit during the check
  my %started;
  foreach my $mysqld ( mysqlds() )
  {
    # Skip if server has been restarted with additional options
    if ( defined $mysqld->{'proc'} && ! exists $mysqld->{'restart_opts'} )
    {
      my $proc= start_check_testcase($tinfo, $mode, $mysqld);
      $started{$proc->pid()}= $proc;
    }
  }

  # Return immediately if no check proceess was started
  return 0 unless ( keys %started );

  my $timeout= start_timer(check_timeout($tinfo));

  while (1){
    my $result;
    my $proc= My::SafeProcess->wait_any_timeout($timeout);
    mtr_report("Got $proc");

    if ( delete $started{$proc->pid()} ) {

      my $err_file= $proc->user_data();
      my $base_file= mtr_match_extension($err_file, "err"); # Trim extension

      # One check testcase process returned
      my $res= $proc->exit_status();

      if ( $res == 0){
	# Check completed without problem

	# Remove the .err file the check generated
	unlink($err_file);

	# Remove the .result file the check generated
	if ( $mode eq 'after' ){
	  unlink("$base_file.result");
	}

	if ( keys(%started) == 0){
	  # All checks completed
	  mark_time_used('check');
	  return 0;
	}
	# Wait for next process to exit
	next;
      }
      else
      {
	if ( $mode eq "after" and $res == 1 )
	{
	  # Test failed, grab the report mysqltest has created
	  my $report= mtr_grab_file($err_file);
	  $tinfo->{check}.=
	    "\nMTR's internal check of the test case '$tname' failed.
This means that the test case does not preserve the state that existed
before the test case was executed.  Most likely the test case did not
do a proper clean-up. It could also be caused by the previous test run
by this thread, if the server wasn't restarted.
This is the diff of the states of the servers before and after the
test case was executed:\n";
	  $tinfo->{check}.= $report;

	  # Check failed, mark the test case with that info
	  $tinfo->{'check_testcase_failed'}= 1;
	  $result= 1;
	}
	elsif ( $res )
	{
	  my $report= mtr_grab_file($err_file);
	  $tinfo->{comment}.=
	    "Could not execute 'check-testcase' $mode ".
	      "testcase '$tname' (res: $res):\n";
	  $tinfo->{comment}.= $report;

	  $result= 2;
	}
        else
        {
          # Remove the .result file the check generated
          unlink("$base_file.result");
        }
	# Remove the .err file the check generated
	unlink($err_file);

      }
    }
    elsif ( $proc->{timeout} ) {
      $tinfo->{comment}.= "Timeout for 'check-testcase' expired after "
	.check_timeout($tinfo)." seconds";
      $result= 4;
    }
    else {
      # Unknown process returned, most likley a crash, abort everything
      $tinfo->{comment}=
	"The server $proc crashed while running ".
	"'check testcase $mode test'".
	get_log_from_proc($proc, $tinfo->{name});
      $result= 3;
    }

    # Kill any check processes still running
    map($_->kill(), values(%started));

    mtr_warning("Check-testcase failed, this could also be caused by the" .
		" previous test run by this worker thread")
      if $result > 1 && $mode eq "before";
    mark_time_used('check');

    return $result;
  }

  mtr_error("INTERNAL_ERROR: check_testcase");
}


# Start run mysqltest on one server
#
# RETURN VALUE
#  0 OK
#  1 Check failed
#
sub start_run_one ($$) {
  my ($mysqld, $run)= @_;

  my $name= "$run-".$mysqld->name();

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));

  mtr_add_arg($args, "--silent");
  mtr_add_arg($args, "--test-file=%s", "include/$run.test");

  my $errfile= "$opt_vardir/tmp/$name.err";
  my $proc= My::SafeProcess->new
    (
     name          => $name,
     path          => $exe_mysqltest,
     error         => $errfile,
     output        => $errfile,
     args          => \$args,
     user_data     => $errfile,
     verbose       => $opt_verbose,
    );
  mtr_verbose("Started $proc");
  return $proc;
}


#
# Run script on all servers, collect results
#
# RETURN VALUE
#  0 ok
#  1 Failure

sub run_on_all($$)
{
  my ($tinfo, $run)= @_;

  # Start the mysqltest processes in parallel to save time
  # also makes it possible to wait for any process to exit during the check
  # and to have a timeout process
  my %started;
  foreach my $mysqld ( mysqlds() )
  {
    if ( defined $mysqld->{'proc'} )
    {
      my $proc= start_run_one($mysqld, $run);
      $started{$proc->pid()}= $proc;
    }
  }

  # Return immediately if no check proceess was started
  return 0 unless ( keys %started );

  my $timeout= start_timer(check_timeout($tinfo));

  while (1){
    my $result;
    my $proc= My::SafeProcess->wait_any_timeout($timeout);
    mtr_report("Got $proc");

    if ( delete $started{$proc->pid()} ) {

      # One mysqltest process returned
      my $err_file= $proc->user_data();
      my $res= $proc->exit_status();

      # Append the report from .err file
      $tinfo->{comment}.= " == $err_file ==\n";
      $tinfo->{comment}.= mtr_grab_file($err_file);
      $tinfo->{comment}.= "\n";

      # Remove the .err file
      unlink($err_file);

      if ( keys(%started) == 0){
	# All completed
	return 0;
      }

      # Wait for next process to exit
      next;
    }
    elsif ($proc->{timeout}) {
      $tinfo->{comment}.= "Timeout for '$run' expired after "
	.check_timeout($tinfo)." seconds";
    }
    else {
      # Unknown process returned, most likley a crash, abort everything
      $tinfo->{comment}.=
	"The server $proc crashed while running '$run'".
	get_log_from_proc($proc, $tinfo->{name});
    }

    # Kill any check processes still running
    map($_->kill(), values(%started));

    return 1;
  }
  mtr_error("INTERNAL_ERROR: run_on_all");
}


sub mark_log {
  my ($log, $tinfo)= @_;
  my $log_msg= "CURRENT_TEST: $tinfo->{name}\n";
  pre_write_errorlog($log, $tinfo->{name});
  mtr_tofile($log, $log_msg);
}


sub find_testcase_skipped_reason($)
{
  my ($tinfo)= @_;

  # Set default message
  $tinfo->{'comment'}= "Detected by testcase(no log file)";

  # Open the test log file
  my $F= IO::File->new($path_current_testlog)
    or return;
  my $reason;

  while ( my $line= <$F> )
  {
    # Look for "reason: <reason for skipping test>"
    if ( $line =~ /reason: (.*)/ )
    {
      $reason= $1;
    }
  }

  if ( ! $reason )
  {
    mtr_warning("Could not find reason for skipping test in $path_current_testlog");
    $reason= "Detected by testcase(reason unknown) ";
  }
  $tinfo->{'comment'}= $reason;
}


sub find_analyze_request
{
  # Open the test log file
  my $F= IO::File->new($path_current_testlog)
    or return;
  my $analyze;

  while ( my $line= <$F> )
  {
    # Look for "reason: <reason for skipping test>"
    if ( $line =~ /analyze: (.*)/ )
    {
      $analyze= $1;
    }
  }

  return $analyze;
}


# Return timezone value of tinfo or default value
sub timezone {
  my ($tinfo)= @_;
  local $_ = $tinfo->{timezone};
  return 'DEFAULT' unless defined $_;
  no warnings 'uninitialized';
  s/\$\{(\w+)\}/$ENV{$1}/ge;
  s/\$(\w+)/$ENV{$1}/ge;
  $_;
}

sub mycnf_create {
  my ($config) = @_;
  my $res;

  foreach my $group ($config->option_groups()) {
    $res .= "[$group->{name}]\n";

    foreach my $option ($group->options()) {
      $res .= $option->name();
      my $value= $option->value();
      if (defined $value) {
	$res .= "=$value";
      }
      $res .= "\n";
    }
    $res .= "\n";
  }
  $res;
}

sub config_files($) {
  my ($tinfo) = @_;
  (
    'my.cnf' => \&mycnf_create,
    $tinfo->{suite}->config_files()
  );
}

sub _like   { return $config ? $config->like($_[0]) : (); }
sub mysqlds { return _like('mysqld\.'); }
sub ndbds   { return _like('cluster_config\.ndbd\.');}
sub ndb_mgmds { return _like('cluster_config\.ndb_mgmd\.'); }

sub fix_servers($) {
  my ($tinfo) = @_;
  return () unless $config;
  my %servers = (
    qr/mysqld\./ => {
      SORT => 300,
      START => \&mysql_server_start,
      WAIT => \&mysql_server_wait,
    },
    qr/mysql_cluster\./ => {
      SORT => 200,
      START => \&ndbcluster_start,
      WAIT => \&ndbcluster_wait_started,
    },
    qr/cluster_config\.ndb_mgmd\./ => {
      SORT => 210,
      START => undef,
    },
    qr/cluster_config\.ndbd\./ => {
      SORT => 220,
      START => undef,
    },
    $tinfo->{suite}->servers()
  );
  for ($config->groups()) {
    while (my ($re,$prop) = each %servers) {
      @$_{keys %$prop} = values %$prop if $_->{name} =~ /^$re/;
    }
  }
}

sub all_servers {
  return unless $config;
  ( sort { $a->{SORT} <=> $b->{SORT} }
       grep { defined $_->{SORT} } $config->groups() );
}

# Storage for changed environment variables
our %old_env;

sub resfile_report_test ($) {
  my $tinfo=  shift;

  resfile_new_test();

  resfile_test_info("name", $tinfo->{name});
  resfile_test_info("variation", $tinfo->{combination})
    if $tinfo->{combination};
  resfile_test_info("start_time", isotime time);
}


#
# Run a single test case
#
# RETURN VALUE
#  0 OK
#  > 0 failure
#

sub run_testcase ($$) {
  my ($tinfo, $server_socket)= @_;
  my $print_freq=20;

  mtr_verbose("Running test:", $tinfo->{name});
  resfile_report_test($tinfo) if $opt_resfile;

  # Allow only alpanumerics pluss _ - + . in combination names,
  # or anything beginning with -- (the latter comes from --combination)
  my $combination= $tinfo->{combination};
  if ($combination && $combination !~ /^\w[-\w\.\+]*$/
                   && $combination !~ /^--/)
  {
    mtr_error("Combination '$combination' contains illegal characters");
  }
  # -------------------------------------------------------
  # Init variables that can change between each test case
  # -------------------------------------------------------
  my $timezone= timezone($tinfo);
  if ($timezone ne 'DEFAULT') {
    $ENV{'TZ'}= $timezone;
  } else {
    delete($ENV{'TZ'});
  }
  $ENV{MTR_SUITE_DIR} = $tinfo->{suite}->{dir};
  mtr_verbose("Setting timezone: $timezone");

  if ( ! using_extern() )
  {
    my @restart= servers_need_restart($tinfo);
    if ( @restart != 0) {
      # Remember that we restarted for this test case (count restarts)
      $tinfo->{'restarted'}= 1;
      stop_servers(reverse @restart);
      if ($opt_warnings) {
        check_warnings_post_shutdown($server_socket);
      }
    }

    if ( started(all_servers()) == 0 )
    {

      # Remove old datadirs
      clean_datadir() unless $opt_start_dirty;

      # Restore old ENV
      while (my ($option, $value)= each( %old_env )) {
	if (defined $value){
	  mtr_verbose("Restoring $option to $value");
	  $ENV{$option}= $value;

	} else {
	  mtr_verbose("Removing $option");
	  delete($ENV{$option});
	}
      }
      %old_env= ();

      mtr_verbose("Generating my.cnf from '$tinfo->{template_path}'");

      # Generate new config file from template
      $config= My::ConfigFactory->new_config
	( {
           testname        => $tinfo->{name},
	   basedir         => $basedir,
	   testdir         => $glob_mysql_test_dir,
	   template_path   => $tinfo->{template_path},
	   extra_template_path => $tinfo->{extra_template_path},
	   vardir          => $opt_vardir,
	   tmpdir          => $opt_tmpdir,
	   baseport        => $baseport,
	   user            => $opt_user,
	   password        => '',
	   ssl             => $opt_ssl_supported,
	   embedded        => $opt_embedded_server,
	  }
	);

      fix_servers($tinfo);

      # Write config files:
      my %config_files = config_files($tinfo);
      while (my ($file, $generate) = each %config_files) {
        next unless $generate;
        my ($path) = "$opt_vardir/$file";
        open (F, '>', $path) or die "Could not open '$path': $!";
        print F &$generate($config);
        close F;
      }

      # Remember current config so a restart can occur when a test need
      # to use a different one
      $current_config_name= $tinfo->{template_path};

      #
      # Set variables in the ENV section
      #
      foreach my $option ($config->options_in_group("ENV"))
      {
	# Save old value to restore it before next time
	$old_env{$option->name()}= $ENV{$option->name()};

	mtr_verbose($option->name(), "=",$option->value());
	$ENV{$option->name()}= $option->value();
      }
    }

    # Write start of testcase to log
    mark_log($path_current_testlog, $tinfo);

    # Make sure the safe_process also exits from now on
    if ($opt_start_exit) {
      My::SafeProcess->start_exit();
    }

    if (start_servers($tinfo))
    {
      report_failure_and_restart($tinfo);
      return 1;
    }
  }
  mark_time_used('restart');

  # --------------------------------------------------------------------
  # If --start or --start-dirty given, stop here to let user manually
  # run tests
  # If --wait-all is also given, do the same, but don't die if one
  # server exits
  # ----------------------------------------------------------------------

  if ( $start_only )
  {
    mtr_print("\nStarted", started(all_servers()));
    mtr_print("Using config for test", $tinfo->{name});
    mtr_print("Port and socket path for server(s):");
    foreach my $mysqld ( mysqlds() )
    {
      mtr_print ($mysqld->name() . "  " . $mysqld->value('port') .
	      "  " . $mysqld->value('socket'));
    }
    if ( $opt_start_exit )
    {
      mtr_print("Server(s) started, not waiting for them to finish");
      if (IS_WINDOWS)
      {
	POSIX::_exit(0);	# exit hangs here in ActiveState Perl
      }
      else
      {
	exit(0);
      }
    }
    mtr_print("Waiting for server(s) to exit...");
    if ( $opt_wait_all ) {
      My::SafeProcess->wait_all();
      mtr_print( "All servers exited" );
      exit(1);
    }
    else {
      my $proc= My::SafeProcess->wait_any();
      if ( grep($proc eq $_, started(all_servers())) )
      {
        mtr_print("Server $proc died");
        exit(1);
      }
      mtr_print("Unknown process $proc died");
      exit(1);
    }
  }

  my $test_timeout= start_timer(testcase_timeout($tinfo));

  do_before_run_mysqltest($tinfo);

  mark_time_used('admin');

  if ( $opt_check_testcases and check_testcase($tinfo, "before") ){
    # Failed to record state of server or server crashed
    report_failure_and_restart($tinfo);

    return 1;
  }

  my $test= $tinfo->{suite}->start_test($tinfo);
  # Set only when we have to keep waiting after expectedly died server
  my $keep_waiting_proc = 0;
  my $print_timeout= start_timer($print_freq * 60);

  while (1)
  {
    my $proc;
    if ($keep_waiting_proc)
    {
      # Any other process exited?
      $proc = My::SafeProcess->check_any();
      if ($proc)
      {
	mtr_verbose ("Found exited process $proc");
      }
      else
      {
	$proc = $keep_waiting_proc;
	# Also check if timer has expired, if so cancel waiting
	if ( has_expired($test_timeout) )
	{
	  $keep_waiting_proc = 0;
	}
      }
    }
    if (! $keep_waiting_proc)
    {
      if($test_timeout > $print_timeout)
      {
         $proc= My::SafeProcess->wait_any_timeout($print_timeout);
         if ( $proc->{timeout} )
         {
            #print out that the test is still on
            mtr_print("Test still running: $tinfo->{name}");
            #reset the timer
            $print_timeout= start_timer($print_freq * 60);
            next;
         }
      }
      else
      {
         $proc= My::SafeProcess->wait_any_timeout($test_timeout);
      }
    }

    # Will be restored if we need to keep waiting
    $keep_waiting_proc = 0;

    unless ( defined $proc )
    {
      mtr_error("wait_any failed");
    }
    mtr_verbose("Got $proc");

    mark_time_used('test');
    # ----------------------------------------------------
    # Was it the test program that exited
    # ----------------------------------------------------
    if ($proc eq $test)
    {
      my $res= $test->exit_status();

      if ($res == 0 and $opt_warnings and check_warnings($tinfo) )
      {
	# Test case suceeded, but it has produced unexpected
	# warnings, continue in $res == 1
	$res= 1;
	resfile_output($tinfo->{'warnings'}) if $opt_resfile;
      }

      if ( $res == 0 )
      {
	my $check_res;
	if ( $opt_check_testcases and
	     $check_res= check_testcase($tinfo, "after"))
	{
	  if ($check_res == 1) {
	    # Test case had sideeffects, not fatal error, just continue
            if ($opt_warnings) {
              # Checking error logs for warnings, so need to stop server
              # gracefully so that memory leaks etc. can be properly detected.
              stop_servers(reverse all_servers());
              check_warnings_post_shutdown($server_socket);
              # Even if we got warnings here, we should not fail this
              # particular test, as the warnings may be caused by an earlier
              # test.
            } else {
              # Not checking warnings, so can do a hard shutdown.
	      stop_all_servers($opt_shutdown_timeout);
            }
	    mtr_report("Resuming tests...\n");
	    resfile_output($tinfo->{'check'}) if $opt_resfile;
	  }
	  else {
	    # Test case check failed fatally, probably a server crashed
	    report_failure_and_restart($tinfo);
	    return 1;
	  }
	}
	mtr_report_test_passed($tinfo);
      }
      elsif ( $res == 62 )
      {
	# Testcase itself tell us to skip this one
	$tinfo->{skip_detected_by_test}= 1;
	# Try to get reason from test log file
	find_testcase_skipped_reason($tinfo);
	mtr_report_test_skipped($tinfo);
	# Restart if skipped due to missing perl, it may have had side effects
	if ( $tinfo->{'comment'} =~ /^perl not found/ )
	{
	  stop_all_servers($opt_shutdown_timeout);
	}
      }
      elsif ( $res == 65 )
      {
	# Testprogram killed by signal
	$tinfo->{comment}=
	  "testprogram crashed(returned code $res)";
	report_failure_and_restart($tinfo);
      }
      elsif ( $res == 1 )
      {
	# Check if the test tool requests that
	# an analyze script should be run
	my $analyze= find_analyze_request();
	if ($analyze){
	  run_on_all($tinfo, "analyze-$analyze");
	}

	# Wait a bit and see if a server died, if so report that instead
	mtr_milli_sleep(100);
	my $srvproc= My::SafeProcess::check_any();
	if ($srvproc && grep($srvproc eq $_, started(all_servers()))) {
	  $proc= $srvproc;
	  goto SRVDIED;
	}

	# Test case failure reported by mysqltest
	report_failure_and_restart($tinfo);
      }
      else
      {
	# mysqltest failed, probably crashed
	$tinfo->{comment}=
	  "mysqltest failed with unexpected return code $res\n";
	report_failure_and_restart($tinfo);
      }

      # Save info from this testcase run to mysqltest.log
      if( -f $path_current_testlog)
      {
	if ($opt_resfile && $res && $res != 62) {
	  resfile_output_file($path_current_testlog);
	}
	mtr_appendfile_to_file($path_current_testlog, $path_testlog);
	unlink($path_current_testlog);
      }

      return ($res == 62) ? 0 : $res;

    }

    # ----------------------------------------------------
    # Check if it was an expected crash
    # ----------------------------------------------------
    my $check_crash = check_expected_crash_and_restart($proc);
    if ($check_crash)
    {
      # Keep waiting if it returned 2, if 1 don't wait or stop waiting.
      $keep_waiting_proc = 0 if $check_crash == 1;
      $keep_waiting_proc = $proc if $check_crash == 2;
      next;
    }

  SRVDIED:
    # ----------------------------------------------------
    # Stop the test case timer
    # ----------------------------------------------------
    $test_timeout= 0;

    # ----------------------------------------------------
    # Check if it was a server that died
    # ----------------------------------------------------
    if ( grep($proc eq $_, started(all_servers())) )
    {
      # Server failed, probably crashed
      $tinfo->{comment}=
	"Server $proc failed during test run" .
	get_log_from_proc($proc, $tinfo->{name});

      # ----------------------------------------------------
      # It's not mysqltest that has exited, kill it
      # ----------------------------------------------------
      $test->kill();

      report_failure_and_restart($tinfo);
      return 1;
    }

    # Try to dump core for mysqltest and all servers
    foreach my $proc ($test, started(all_servers())) 
    {
      mtr_print("Trying to dump core for $proc");
      if ($proc->dump_core())
      {
	$proc->wait_one(20);
      }
    }

    # ----------------------------------------------------
    # It's not mysqltest that has exited, kill it
    # ----------------------------------------------------
    $test->kill();

    # ----------------------------------------------------
    # Check if testcase timer expired
    # ----------------------------------------------------
    if ( $proc->{timeout} )
    {
      my $log_file_name= $opt_vardir."/log/".$tinfo->{shortname}.".log";
      $tinfo->{comment}=
        "Test case timeout after ".testcase_timeout($tinfo).
	  " seconds\n\n";
      # Add 20 last executed commands from test case log file
      if  (-e $log_file_name)
      {
        $tinfo->{comment}.=
	   "== $log_file_name == \n".
	     mtr_lastlinesfromfile($log_file_name, 20)."\n";
      }
      $tinfo->{'timeout'}= testcase_timeout($tinfo); # Mark as timeout
      run_on_all($tinfo, 'analyze-timeout');

      report_failure_and_restart($tinfo);
      return 1;
    }

    mtr_error("Unhandled process $proc exited");
  }
  mtr_error("Should never come here");
}


# Keep track of last position in mysqld error log where we scanned for
# warnings, so we can attribute any warnings found to the correct test
# suite or server restart.
our $last_warning_position= { };

# Called just before a mysqld server is started or a testcase is run,
# to keep track of which tests have been run since last restart, and
# of when the error log is reset.
#
# Second argument $test_name is test name, or undef for server restart.
sub pre_write_errorlog {
  my ($error_log, $test_name)= @_;

  if (! -e $error_log) {
    # If the error log is moved away, reset the warning parse position.
    delete $last_warning_position->{$error_log};
  }

  if (defined($test_name)) {
    $last_warning_position->{$error_log}{test_names}= []
      unless exists($last_warning_position->{$error_log}{test_names});
    push @{$last_warning_position->{$error_log}{test_names}}, $test_name;
  } else {
    # Server restart, so clear the list of tests run since last restart.
    # (except the last one (if any), which is the test about to be run).
    if (defined($last_warning_position->{$error_log}{test_names}) &&
        @{$last_warning_position->{$error_log}{test_names}}) {
      $last_warning_position->{$error_log}{test_names}=
        [$last_warning_position->{$error_log}{test_names}[-1]];
    } else {
      $last_warning_position->{$error_log}{test_names}= [];
    }
  }
}

# Extract server log from after the last occurrence of named test
# Return as an array of lines
#

sub extract_server_log ($$) {
  my ($error_log, $tname) = @_;
  
  return unless $error_log;

  # Open the servers .err log file and read all lines
  # belonging to current test into @lines
  my $Ferr = IO::File->new($error_log)
    or mtr_error("Could not open file '$error_log' for reading: $!");

  my @lines;
  my $found_test= 0;		# Set once we've found the log of this test
  while ( my $line = <$Ferr> )
  {
    if ($found_test)
    {
      # If test wasn't last after all, discard what we found, test again.
      if ( $line =~ /^CURRENT_TEST:/)
      {
	@lines= ();
	$found_test= $line =~ /^CURRENT_TEST: $tname/;
      }
      else
      {
	push(@lines, $line);
	if (scalar(@lines) > 1000000) {
	  $Ferr = undef;
	  mtr_warning("Too much log from test, bailing out from extracting");
	  return ();
	}
      }
    }
    else
    {
      # Search for beginning of test, until found
      $found_test= 1 if ($line =~ /^CURRENT_TEST: $tname/);
    }
  }
  $Ferr = undef; # Close error log file

  return @lines;
}

# Get log from server identified from its $proc object, from named test
# Return as a single string
#

sub get_log_from_proc ($$) {
  my ($proc, $name)= @_;
  my $srv_log= "";

  foreach my $mysqld (all_servers()) {
    if ($mysqld->{proc} eq $proc) {
      my @srv_lines= extract_server_log($mysqld->if_exist('#log-error'), $name);
      $srv_log= "\nServer log from this test:\n" .
	"----------SERVER LOG START-----------\n". join ("", @srv_lines) .
	"----------SERVER LOG END-------------\n";
      last;
    }
  }
  return $srv_log;
}

#
# Perform a rough examination of the servers
# error log and write all lines that look
# suspicious into $error_log.warnings
#

sub extract_warning_lines ($$) {
  my ($error_log, $append) = @_;

  # Open the servers .err log file and read all lines
  # belonging to current tets into @lines
  my $Ferr = IO::File->new($error_log)
    or return [];
  my $last_pos= $last_warning_position->{$error_log}{seek_pos};
  $Ferr->seek($last_pos, 0) if defined($last_pos);
  # If the seek fails, we will parse the whole error log, at least we will not
  # miss any warnings.

  my @lines= <$Ferr>;
  $last_warning_position->{$error_log}{seek_pos}= $Ferr->tell();
  $Ferr = undef; # Close error log file

  # mysql_client_test.test sends a COM_DEBUG packet to the server
  # to provoke a safemalloc leak report, ignore any warnings
  # between "Begin/end safemalloc memory dump"
  if ( grep(/Begin safemalloc memory dump:/, @lines) > 0)
  {
    my $discard_lines= 1;
    foreach my $line ( @lines )
    {
      if ($line =~ /Begin safemalloc memory dump:/){
	$discard_lines = 1;
      } elsif ($line =~ /End safemalloc memory dump./){
	$discard_lines = 0;
      }

      if ($discard_lines){
	$line = "ignored";
      }
    }
  }

  # Write all suspicious lines to $error_log.warnings file
  my $warning_log = "$error_log.warnings";
  my $Fwarn = IO::File->new($warning_log, $append ? "a+" : "w")
    or die("Could not open file '$warning_log' for writing: $!");
  if (!$append)
  {
    print $Fwarn "Suspicious lines from $error_log\n";
  }

  my @patterns =
    (
     qr/^Warning|mysqld: Warning|\[Warning\]/,
     qr/^Error:|\[ERROR\]/,
     qr/^==\d+==\s+\S/, # valgrind errors
     qr/InnoDB: Warning|InnoDB: Error/,
     qr/^safe_mutex:|allocated at line/,
     qr/missing DBUG_RETURN/,
     qr/Attempting backtrace/,
     qr/Assertion .* failed/,
    );
  # These are taken from the include/mtr_warnings.sql global suppression
  # list. They occur delayed, so they can be parsed during shutdown rather
  # than during the per-test check.
  #
  # ToDo: having the warning suppressions inside the mysqld we are trying to
  # check is in any case horrible. We should change it to all be done here
  # within the Perl code, which is both simpler, easier, faster, and more
  # robust. We could still have individual test cases put in suppressions by
  # parsing statically or by writing dynamically to a CSV table read by the
  # Perl code.
  my @antipatterns =
    (
     qr/error .*connecting to master/,
     qr/Plugin 'ndbcluster' will be forced to shutdown/,
     qr/InnoDB: Error: in ALTER TABLE `test`.`t[12]`/,
     qr/InnoDB: Error: table `test`.`t[12]` .*does not exist in the InnoDB internal/,
     qr/InnoDB: Warning: a long semaphore wait:/,
     qr/Slave: Unknown table 't1' Error_code: 1051/,
     qr/Slave SQL:.*(Error_code: [[:digit:]]+|Query:.*)/,
     qr/slave SQL thread aborted/,
     qr/unknown option '--loose[-_]/,
     qr/unknown variable 'loose[-_]/,
     qr/Invalid .*old.* table or database name/,
     qr/Now setting lower_case_table_names to [02]/,
     qr/Setting lower_case_table_names=2/,
     qr/You have forced lower_case_table_names to 0/,
     qr/Plugin 'ndbcluster' will be forced to shutdow/,
     qr/deprecated/,
     qr/Slave SQL thread retried transaction/,
     qr/Slave \(additional info\)/,
     qr/Incorrect information in file/,
     qr/Incorrect key file for table .*crashed.*/,
     qr/Slave I\/O: Get master SERVER_ID failed with error:.*/,
     qr/Slave I\/O: Get master clock failed with error:.*/,
     qr/Slave I\/O: Get master COLLATION_SERVER failed with error:.*/,
     qr/Slave I\/O: Get master TIME_ZONE failed with error:.*/,
     qr/Slave I\/O: error reconnecting to master '.*' - retry-time: [1-3]  retries/,
     qr/Slave I\/0: Master command COM_BINLOG_DUMP failed/,
     qr/Error reading packet/,
     qr/Lost connection to MySQL server at 'reading initial communication packet'/,
     qr/Failed on request_dump/,
     qr/Slave: Can't drop database.* database doesn't exist/,
     qr/Slave: Operation DROP USER failed for 'create_rout_db'/,
     qr|Checking table:   '\..mtr.test_suppressions'|,
     qr|Table \./test/bug53592 has a primary key in InnoDB data dictionary, but not in MySQL|,
     qr|Table '\..mtr.test_suppressions' is marked as crashed and should be repaired|,
     qr|Can't open shared library.*ha_archive|,
     qr|InnoDB: Error: table 'test/bug39438'|,
     qr| entry '.*' ignored in --skip-name-resolve mode|,
     qr|mysqld got signal 6|,
     qr|Error while setting value 'pool-of-threads' to 'thread_handling'|,
     qr|Access denied for user|,
     qr|Aborted connection|,
     qr|table.*is full|,
     qr|Linux Native AIO|, # warning that aio does not work on /dev/shm
     qr|Error: io_setup\(\) failed|,
     qr|Warning: io_setup\(\) failed|,
     qr|Warning: io_setup\(\) attempt|,
     qr|setrlimit could not change the size of core files to 'infinity';|,
     qr|feedback plugin: failed to retrieve the MAC address|,
     qr|Plugin 'FEEDBACK' init function returned error|,
     qr|Plugin 'FEEDBACK' registration as a INFORMATION SCHEMA failed|,
     qr|Failed to setup SSL|,
     qr|SSL error: Failed to set ciphers to use|,
     qr/Plugin 'InnoDB' will be forced to shutdown/,
    );

  my $matched_lines= [];
  LINE: foreach my $line ( @lines )
  {
    PAT: foreach my $pat ( @patterns )
    {
      if ( $line =~ /$pat/ )
      {
        foreach my $apat (@antipatterns)
        {
          next LINE if $line =~ $apat;
        }
	print $Fwarn $line;
        push @$matched_lines, $line;
	last PAT;
      }
    }
  }
  $Fwarn = undef; # Close file

  if (scalar(@$matched_lines) > 0 &&
      defined($last_warning_position->{$error_log}{test_names})) {
    return ($last_warning_position->{$error_log}{test_names}, $matched_lines);
  } else {
    return ([], $matched_lines);
  }
}


# Run include/check-warnings.test
#
# RETURN VALUE
#  0 OK
#  1 Check failed
#
sub start_check_warnings ($$) {
  my $tinfo=    shift;
  my $mysqld=   shift;

  my $name= "warnings-".$mysqld->name();

  my $log_error= $mysqld->value('#log-error');
  # To be communicated to the test
  $ENV{MTR_LOG_ERROR}= $log_error;
  extract_warning_lines($log_error, 0);

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));
  mtr_add_arg($args, "--test-file=%s", "include/check-warnings.test");

  if ( $opt_embedded_server )
  {

    # Get the args needed for the embedded server
    # and append them to args prefixed
    # with --sever-arg=

    my $mysqld=  $config->group('embedded')
      or mtr_error("Could not get [embedded] section");

    my $mysqld_args;
    mtr_init_args(\$mysqld_args);
    my $extra_opts= get_extra_opts($mysqld, $tinfo);
    mysqld_arguments($mysqld_args, $mysqld, $extra_opts);
    mtr_add_arg($args, "--server-arg=%s", $_) for @$mysqld_args;
  }

  my $errfile= "$opt_vardir/tmp/$name.err";
  my $proc= My::SafeProcess->new
    (
     name          => $name,
     path          => $exe_mysqltest,
     error         => $errfile,
     output        => $errfile,
     args          => \$args,
     user_data     => [$errfile, $mysqld],
     verbose       => $opt_verbose,
    );
  mtr_verbose("Started $proc");
  return $proc;
}


#
# Loop through our list of processes and check the error log
# for unexpected errors and warnings
#
sub check_warnings ($) {
  my ($tinfo)= @_;
  my $res= 0;

  my $tname= $tinfo->{name};

  # Clear previous warnings
  delete($tinfo->{warnings});

  # Start the mysqltest processes in parallel to save time
  # also makes it possible to wait for any process to exit during the check
  my %started;
  foreach my $mysqld ( mysqlds() )
  {
    if ( defined $mysqld->{'proc'} )
    {
      my $proc= start_check_warnings($tinfo, $mysqld);
      $started{$proc->pid()}= $proc;
    }
  }

  # Return immediately if no check proceess was started
  return 0 unless ( keys %started );

  my $timeout= start_timer(check_timeout($tinfo));

  while (1){
    my $result= 0;
    my $proc= My::SafeProcess->wait_any_timeout($timeout);
    mtr_report("Got $proc");

    if ( delete $started{$proc->pid()} ) {
      # One check warning process returned
      my $res= $proc->exit_status();
      my ($err_file, $mysqld)= @{$proc->user_data()};

      if ( $res == 0 or $res == 62 ){

	if ( $res == 0 ) {
	  # Check completed with problem
	  my $report= mtr_grab_file($err_file);
	  # Log to var/log/warnings file
	  mtr_tofile("$opt_vardir/log/warnings",
		     $tname."\n".$report);

	  $tinfo->{'warnings'}.= $report;
	  $result= 1;
	}

	if ( $res == 62 ) {
	  # Test case was ok and called "skip"
	  # Remove the .err file the check generated
	  unlink($err_file);
	}

	if ( keys(%started) == 0){
	  # All checks completed
	  mark_time_used('ch-warn');
	  return $result;
	}
	# Wait for next process to exit
	next;
      }
      else
      {
	my $report= mtr_grab_file($err_file);
	$tinfo->{comment}.=
	  "Could not execute 'check-warnings' for ".
	    "testcase '$tname' (res: $res) server: '".
              $mysqld->name() .":\n";
	$tinfo->{comment}.= $report;

	$result= 2;
      }
    }
    elsif ( $proc->{timeout} ) {
      $tinfo->{comment}.= "Timeout for 'check warnings' expired after "
	.check_timeout($tinfo)." seconds";
      $result= 4;
    }
    else {
      # Unknown process returned, most likley a crash, abort everything
      $tinfo->{comment}=
	"The server $proc crashed while running 'check warnings'".
	get_log_from_proc($proc, $tinfo->{name});
      $result= 3;
    }

    # Kill any check processes still running
    map($_->kill(), values(%started));

    mark_time_used('ch-warn');
    return $result;
  }

  mtr_error("INTERNAL_ERROR: check_warnings");
}

# Check for warnings generated during shutdown of a mysqld server.
# If any, report them to master server, and return true; else just return
# false.

sub check_warnings_post_shutdown {
  my ($server_socket)= @_;
  my $testname_hash= { };
  my $report= '';
  foreach my $mysqld ( mysqlds())
  {
    my ($testlist, $match_lines)=
        extract_warning_lines($mysqld->value('#log-error'), 1);
    $testname_hash->{$_}= 1 for @$testlist;
    $report.= join('', @$match_lines);
  }
  my @warning_tests= keys(%$testname_hash);
  if (@warning_tests) {
    my $fake_test= My::Test->new(testnames => \@warning_tests);
    $fake_test->{'warnings'}= $report;
    $fake_test->write_test($server_socket, 'WARNINGS');
  }
}

#
# Loop through our list of processes and look for and entry
# with the provided pid, if found check for the file indicating
# expected crash and restart it.
#
sub check_expected_crash_and_restart {
  my ($proc)= @_;

  foreach my $mysqld ( mysqlds() )
  {
    next unless ( $mysqld->{proc} and $mysqld->{proc} eq $proc );

    # Check if crash expected by looking at the .expect file
    # in var/tmp
    my $expect_file= "$opt_vardir/tmp/".$mysqld->name().".expect";
    if ( -f $expect_file )
    {
      mtr_verbose("Crash was expected, file '$expect_file' exists");

      for (my $waits = 0;  $waits < 50;  mtr_milli_sleep(100), $waits++)
      {
	# Race condition seen on Windows: try again until file not empty
	next if -z $expect_file;
	# If last line in expect file starts with "wait"
	# sleep a little and try again, thus allowing the
	# test script to control when the server should start
	# up again. Keep trying for up to 5s at a time.
	my $last_line= mtr_lastlinesfromfile($expect_file, 1);
	if ($last_line =~ /^wait/ )
	{
	  mtr_verbose("Test says wait before restart") if $waits == 0;
	  next;
	}

	# Ignore any partial or unknown command
	next unless $last_line =~ /^restart/;
	# If last line begins "restart:", the rest of the line is read as
        # extra command line options to add to the restarted mysqld.
        # Anything other than 'wait' or 'restart:' (with a colon) will
        # result in a restart with original mysqld options.
	if ($last_line =~ /restart:(.+)/) {
	  my @rest_opt= split(' ', $1);
	  $mysqld->{'restart_opts'}= \@rest_opt;
	} else {
	  delete $mysqld->{'restart_opts'};
	}
	unlink($expect_file);

	# Start server with same settings as last time
	mysqld_start($mysqld, $mysqld->{'started_opts'});

	return 1;
      }
      # Loop ran through: we should keep waiting after a re-check
      return 2;
    }
  }

  # Not an expected crash
  return 0;
}


# Remove all files and subdirectories of a directory
sub clean_dir {
  my ($dir)= @_;
  mtr_verbose("clean_dir: $dir");
  finddepth(
	  { no_chdir => 1,
	    wanted => sub {
	      if (-d $_){
		# A dir
		if ($_ eq $dir){
		  # The dir to clean
		  return;
		} else {
		  mtr_verbose("rmdir: '$_'");
		  rmdir($_) or mtr_warning("rmdir($_) failed: $!");
		}
	      } else {
		# Hopefully a file
		mtr_verbose("unlink: '$_'");
		unlink($_) or mtr_warning("unlink($_) failed: $!");
	      }
	    }
	  },
	    $dir);
}


sub clean_datadir {
  mtr_verbose("Cleaning datadirs...");

  if (started(all_servers()) != 0){
    mtr_error("Trying to clean datadir before all servers stopped");
  }

  for (all_servers())
  {
    my $dir= "$opt_vardir/".$_->{name};
    mtr_verbose(" - removing '$dir'");
    rmtree($dir);
  }

  # Remove all files in tmp and var/tmp
  clean_dir("$opt_vardir/tmp");
  if ($opt_tmpdir ne "$opt_vardir/tmp"){
    clean_dir($opt_tmpdir);
  }
}


#
# Save datadir before it's removed
#
sub save_datadir_after_failure($$) {
  my ($dir, $savedir)= @_;

  mtr_report(" - saving '$dir'");
  my $dir_name= basename($dir);
  rename("$dir", "$savedir/$dir_name");
}


sub after_failure ($) {
  my ($tinfo)= @_;

  mtr_report("Saving datadirs...");

  my $save_dir= "$opt_vardir/log/";
  $save_dir.= $tinfo->{name};
  # Add combination name if any
  $save_dir.= '-' . join(',', sort @{$tinfo->{combinations}})
    if defined $tinfo->{combinations};

  # Save savedir  path for server
  $tinfo->{savedir}= $save_dir;

  mkpath($save_dir) if ! -d $save_dir;

  #
  # Create a log of files in vardir (good for buildbot)
  #
  if (!IS_WINDOWS)
  {
    my $Flog= IO::File->new("$opt_vardir/log/files.log", "w");
    if ($Flog)
    {
      print $Flog scalar(`/bin/ls -Rl $opt_vardir/*`);
      close($Flog);
    }
  }

  # Save the used config files
  my %config_files = config_files($tinfo);
  while (my ($file, $generate) = each %config_files) {
    copy("$opt_vardir/$file", $save_dir);
  }

  # Copy the tmp dir
  copytree("$opt_vardir/tmp/", "$save_dir/tmp/");

  foreach (all_servers()) {
    my $dir= "$opt_vardir/".$_->{name};
    save_datadir_after_failure($dir, $save_dir);
  }
}


sub report_failure_and_restart ($) {
  my $tinfo= shift;

  if ($opt_valgrind_mysqld && ($tinfo->{'warnings'} || $tinfo->{'timeout'})) {
    # In these cases we may want valgrind report from normal termination
    $tinfo->{'dont_kill_server'}= 1;
  }
  # Shotdown properly if not to be killed (for valgrind)
  stop_all_servers($tinfo->{'dont_kill_server'} ? $opt_shutdown_timeout : 0);

  $tinfo->{'result'}= 'MTR_RES_FAILED';

  my $test_failures= $tinfo->{'failures'} || 0;
  $tinfo->{'failures'}=  $test_failures + 1;


  if ( $tinfo->{comment} )
  {
    # The test failure has been detected by mysql-test-run.pl
    # when starting the servers or due to other error, the reason for
    # failing the test is saved in "comment"
    ;
  }

  if ( !defined $tinfo->{logfile} )
  {
    my $logfile= $path_current_testlog;
    if ( defined $logfile )
    {
      if ( -f $logfile )
      {
	# Test failure was detected by test tool and its report
	# about what failed has been saved to file. Save the report
	# in tinfo
	$tinfo->{logfile}= mtr_fromfile($logfile);
	# If no newlines in the test log:
	# (it will contain the CURRENT_TEST written by mtr, so is not empty)
	if ($tinfo->{logfile} !~ /\n/)
	{
	  # Show how far it got before suddenly failing
	  $tinfo->{comment}.= "mysqltest failed but provided no output\n";
	  my $log_file_name= $opt_vardir."/log/".$tinfo->{shortname}.".log";
	  if (-e $log_file_name) {
	    $tinfo->{comment}.=
	      "The result from queries just before the failure was:".
	      "\n< snip >\n".
	      mtr_lastlinesfromfile($log_file_name, 20)."\n";
	  }
	}
      }
      else
      {
	# The test tool report didn't exist, display an
	# error message
	$tinfo->{logfile}= "Could not open test tool report '$logfile'";
      }
    }
  }

  after_failure($tinfo);

  mtr_report_test($tinfo);

}


sub run_system(@) {
  mtr_verbose("Running '$_[0]'");
  my $ret= system(@_) >> 8;
  return $ret;
}


sub mysqld_stop {
  my $mysqld= shift or die "usage: mysqld_stop(<mysqld>)";

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--character-sets-dir=%s", $mysqld->value('character-sets-dir'));
  mtr_add_arg($args, "--user=%s", $opt_user);
  mtr_add_arg($args, "--password=");
  mtr_add_arg($args, "--port=%d", $mysqld->value('port'));
  mtr_add_arg($args, "--host=%s", $mysqld->value('#host'));
  mtr_add_arg($args, "--connect_timeout=20");
  mtr_add_arg($args, "--protocol=tcp");

  mtr_add_arg($args, "shutdown");

  My::SafeProcess->run
    (
     name          => "mysqladmin shutdown ".$mysqld->name(),
     path          => $exe_mysqladmin,
     args          => \$args,
     error         => "$opt_vardir/log/mysqladmin.err",

    );
}


sub mysqld_arguments ($$$) {
  my $args=              shift;
  my $mysqld=            shift;
  my $extra_opts=        shift;

  my @defaults = grep(/^--defaults-file=/, @$extra_opts);
  if (@defaults > 0) {
    mtr_add_arg($args, pop(@defaults))
  }
  else {
    mtr_add_arg($args, "--defaults-file=%s",  $path_config_file);
  }

  # When mysqld is run by a root user(euid is 0), it will fail
  # to start unless we specify what user to run as, see BUG#30630
  my $euid= $>;
  if (!IS_WINDOWS and $euid == 0 and
      (grep(/^--user/, @$extra_opts)) == 0) {
    mtr_add_arg($args, "--user=root");
  }

  if (!using_extern() and !$opt_user_args)
  {
    # Turn on logging to file
    mtr_add_arg($args, "%s--log-output=file");
  }

  # Check if "extra_opt" contains --log-bin
  my $skip_binlog= not grep /^--(loose-)?log-bin/, @$extra_opts;

  # Indicate to mysqld it will be debugged in debugger
  if ( $glob_debugger )
  {
    mtr_add_arg($args, "--gdb");
  }

  my $found_skip_core= 0;
  my @plugins;
  my %seen;
  my $plugin;
  foreach my $arg ( @$extra_opts )
  {
    # Skip --defaults-file option since it's handled above.
    next if $arg =~ /^--defaults-file/;

    # Allow --skip-core-file to be set in <testname>-[master|slave].opt file
    if ($arg eq "--skip-core-file")
    {
      $found_skip_core= 1;
    }
    elsif ($skip_binlog and mtr_match_prefix($arg, "--binlog-format"))
    {
      ; # Dont add --binlog-format when running without binlog
    }
    elsif ($arg eq "--loose-skip-log-bin" and
           $mysqld->option("log-slave-updates"))
    {
      ; # Dont add --skip-log-bin when mysqld have --log-slave-updates in config
    }
    elsif ($plugin = mtr_match_prefix($arg,  "--plugin-load="))
    {
      next if $plugin =~ /=$/;
      push @plugins, $plugin unless $seen{$plugin};
      $seen{$plugin} = 1;
    }
    else
    {
      mtr_add_arg($args, "%s", $arg);
    }
  }
  $opt_skip_core = $found_skip_core;
  if ( !$found_skip_core && !$opt_user_args )
  {
    mtr_add_arg($args, "%s", "--core-file");
  }

  # Enable the debug sync facility, set default wait timeout.
  # Facility stays disabled if timeout value is zero.
  mtr_add_arg($args, "--loose-debug-sync-timeout=%s",
              $opt_debug_sync_timeout) unless $opt_user_args;

  if (@plugins) {
    my $sep = (IS_WINDOWS) ? ';' : ':';
    mtr_add_arg($args, "--plugin-load=%s" .  join($sep, @plugins));
  }

  return $args;
}



sub mysqld_start ($$) {
  my $mysqld=            shift;
  my $extra_opts=        shift;

  mtr_verbose(My::Options::toStr("mysqld_start", @$extra_opts));

  my $exe= find_mysqld($mysqld->value('basedir'));
  my $wait_for_pid_file= 1;

  mtr_error("Internal error: mysqld should never be started for embedded")
    if $opt_embedded_server;

  my $args;
  mtr_init_args(\$args);

  if ( $opt_valgrind_mysqld )
  {
    valgrind_arguments($args, \$exe);
  }
  if ( $opt_strace)
  {
    strace_arguments($args, \$exe, $mysqld->name());
  }

  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));

  # Add any additional options from an in-test restart
  my @all_opts= @$extra_opts;
  if (exists $mysqld->{'restart_opts'}) {
    push (@all_opts, @{$mysqld->{'restart_opts'}});
    mtr_verbose(My::Options::toStr("mysqld_start restart",
				   @{$mysqld->{'restart_opts'}}));
  }
  mysqld_arguments($args,$mysqld,\@all_opts);

  if ( $opt_debug )
  {
    mtr_add_arg($args, "--debug-dbug=$debug_d:t:i:A,%s/log/%s.trace",
		$path_vardir_trace, $mysqld->name());
  }

  if (IS_WINDOWS)
  {
    # Trick the server to send output to stderr, with --console
    if (!(grep(/^--log-error/, @$args))) {
      mtr_add_arg($args, "--console");
    }
  }

  if ( $opt_gdb || $opt_manual_gdb )
  {
    gdb_arguments(\$args, \$exe, $mysqld->name());
  }
  elsif ( $opt_manual_lldb )
  {
    lldb_arguments(\$args, \$exe, $mysqld->name());
  }
  elsif ( $opt_ddd || $opt_manual_ddd )
  {
    ddd_arguments(\$args, \$exe, $mysqld->name());
  }
  elsif ( $opt_dbx || $opt_manual_dbx ) {
    dbx_arguments(\$args, \$exe, $mysqld->name());
  }
  elsif ( $opt_debugger )
  {
    debugger_arguments(\$args, \$exe, $mysqld->name());
  }
  elsif ( $opt_manual_debug )
  {
     print "\nStart " .$mysqld->name()." in your debugger\n" .
           "dir: $glob_mysql_test_dir\n" .
           "exe: $exe\n" .
	   "args:  " . join(" ", @$args)  . "\n\n" .
	   "Waiting ....\n";

     # Indicate the exe should not be started
    $exe= undef;
  }
  else
  {
    # Default to not wait until pid file has been created
    $wait_for_pid_file= 0;
  }

  # Remove the old pidfile if any
  unlink($mysqld->value('pid-file'));

  my $output= $mysqld->value('#log-error');

  if ( $opt_valgrind and $opt_debug )
  {
    # When both --valgrind and --debug is selected, send
    # all output to the trace file, making it possible to
    # see the exact location where valgrind complains

    # Write a message about this to the normal log file
    my $trace_name= "$opt_vardir/log/".$mysqld->name().".trace";
    mtr_tofile($output,
               "NOTE: When running with --valgrind --debug the output from ",
	       "mysqld (where the valgrind messages shows up) is stored ",
	       "together with the trace file to make it ",
	       "easier to find the exact position of valgrind errors.",
	       "See trace file $trace_name.\n");
    $output= $trace_name;

  }
  # Remember this log file for valgrind error report search
  $mysqld_logs{$output}= 1 if $opt_valgrind;
  # Remember data dir for gmon.out files if using gprof
  $gprof_dirs{$mysqld->value('datadir')}= 1 if $opt_gprof;

  if ( defined $exe )
  {
    pre_write_errorlog($output);
    $mysqld->{'proc'}= My::SafeProcess->new
      (
       name          => $mysqld->name(),
       path          => $exe,
       args          => \$args,
       output        => $output,
       error         => $output,
       append        => 1,
       verbose       => $opt_verbose,
       nocore        => $opt_skip_core,
       host          => undef,
       shutdown      => sub { mysqld_stop($mysqld) },
       envs          => \@opt_mysqld_envs,
      );
    mtr_verbose("Started $mysqld->{proc}");
  }

  if ( $wait_for_pid_file &&
       !sleep_until_file_created($mysqld->value('pid-file'),
				 $opt_start_timeout,
				 $mysqld->{'proc'},
                                 $warn_seconds))
  {
    my $mname= $mysqld->name();
    mtr_error("Failed to start mysqld $mname with command $exe");
  }

  # Remember options used when starting
  $mysqld->{'started_opts'}= $extra_opts;

  return;
}


sub stop_all_servers () {
  my $shutdown_timeout = $_[0] or 0;

  mtr_verbose("Stopping all servers...");

  # Kill all started servers
  My::SafeProcess::shutdown($shutdown_timeout,
			    started(all_servers()));

  # Remove pidfiles
  foreach my $server ( all_servers() )
  {
    my $pid_file= $server->if_exist('pid-file');
    unlink($pid_file) if defined $pid_file;
  }

  # Mark servers as stopped
  map($_->{proc}= undef, all_servers());

}


# Find out if server should be restarted for this test
sub server_need_restart {
  my ($tinfo, $server)= @_;

  if ( using_extern() )
  {
    mtr_verbose_restart($server, "no restart for --extern server");
    return 0;
  }

  if ( $opt_force_restart ) {
    mtr_verbose_restart($server, "forced restart turned on");
    return 1;
  }

  if ( $tinfo->{template_path} ne $current_config_name)
  {
    mtr_verbose_restart($server, "using different config file");
    return 1;
  }

  if ( $tinfo->{'master_sh'}  || $tinfo->{'slave_sh'} )
  {
    mtr_verbose_restart($server, "sh script to run");
    return 1;
  }

  if ( ! started($server) )
  {
    mtr_verbose_restart($server, "not started");
    return 1;
  }

  my $started_tinfo= $server->{'started_tinfo'};
  if ( defined $started_tinfo )
  {

    # Check if timezone of  test that server was started
    # with differs from timezone of next test
    if ( timezone($started_tinfo) ne timezone($tinfo) )
    {
      mtr_verbose_restart($server, "different timezone");
      return 1;
    }
  }

  if ($server->name() =~ /^mysqld\./)
  {

    # Check that running process was started with same options
    # as the current test requires
    my $extra_opts= get_extra_opts($server, $tinfo);
    my $started_opts= $server->{'started_opts'};

    # Also, always restart if server had been restarted with additional
    # options within test.
    if (!My::Options::same($started_opts, $extra_opts) ||
        exists $server->{'restart_opts'})
    {
      my $use_dynamic_option_switch= 0;
      if (!$use_dynamic_option_switch)
      {
	mtr_verbose_restart($server, "running with different options '" .
			    join(" ", @{$extra_opts}) . "' != '" .
			    join(" ", @{$started_opts}) . "'" );
	return 1;
      }

      mtr_verbose(My::Options::toStr("started_opts", @$started_opts));
      mtr_verbose(My::Options::toStr("extra_opts", @$extra_opts));

      # Get diff and check if dynamic switch is possible
      my @diff_opts= My::Options::diff($started_opts, $extra_opts);
      mtr_verbose(My::Options::toStr("diff_opts", @diff_opts));

      my $query= My::Options::toSQL(@diff_opts);
      mtr_verbose("Attempting dynamic switch '$query'");
      if (run_query($tinfo, $server, $query)){
	mtr_verbose("Restart: running with different options '" .
		    join(" ", @{$extra_opts}) . "' != '" .
		    join(" ", @{$started_opts}) . "'" );
	return 1;
      }

      # Remember the dynamically set options
      $server->{'started_opts'}= $extra_opts;
    }
  }

  # Default, no restart
  return 0;
}


sub servers_need_restart($) {
  my ($tinfo)= @_;
  return grep { server_need_restart($tinfo, $_); } all_servers();
}



############################################

############################################

#
# Filter a list of servers and return only those that are part
# of the specified cluster
#
sub in_cluster {
  my ($cluster)= shift;
  # Return only processes for a specific cluster
  return grep { $_->suffix() eq $cluster->suffix() } @_;
}



#
# Filter a list of servers and return the SafeProcess
# for only those that are started or stopped
#
sub started { return grep(defined $_, map($_->{proc}, @_));  }
sub stopped { return grep(!defined $_, map($_->{proc}, @_)); }


sub get_extra_opts {
  # No extra options if --user-args
  return \@opt_extra_mysqld_opt if $opt_user_args;

  my ($mysqld, $tinfo)= @_;

  my $opts=
    $mysqld->option("#!use-slave-opt") ?
      $tinfo->{slave_opt} : $tinfo->{master_opt};

  # Expand environment variables
  foreach my $opt ( @$opts )
  {
    no warnings 'uninitialized';
    $opt =~ s/\$\{(\w+)\}/$ENV{$1}/ge;
    $opt =~ s/\$(\w+)/$ENV{$1}/ge;
  }
  return $opts;
}


sub stop_servers($$) {
  my (@servers)= @_;

  mtr_report("Restarting ", started(@servers));

  My::SafeProcess::shutdown($opt_shutdown_timeout,
                             started(@servers));

  foreach my $server (@servers)
  {
    # Mark server as stopped
    $server->{proc}= undef;

    # Forget history
    delete $server->{'started_tinfo'};
    delete $server->{'started_opts'};
    delete $server->{'started_cnf'};
  }
}


#
# start_servers
#
# Start servers not already started
#
# RETURN
#  0 OK
#  1 Start failed
#
sub start_servers($) {
  my ($tinfo)= @_;

  for (all_servers()) {
    $_->{START}->($_, $tinfo) if $_->{START};
  }

  for (all_servers()) {
    next unless $_->{WAIT} and started($_);
    if ($_->{WAIT}->($_)) {
      $tinfo->{comment}= "Failed to start ".$_->name() . "\n";
      return 1;
    }
  }
  return 0;
}


#
# Run include/check-testcase.test
# Before a testcase, run in record mode and save result file to var/tmp
# After testcase, run and compare with the recorded file, they should be equal!
#
# RETURN VALUE
#  The newly started process
#
sub start_check_testcase ($$$) {
  my $tinfo=    shift;
  my $mode=     shift;
  my $mysqld=   shift;

  my $name= "check-".$mysqld->name();
  # Replace dots in name with underscore to avoid that mysqltest
  # misinterpret's what the filename extension is :(
  $name=~ s/\./_/g;

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));
  mtr_add_arg($args, "--result-file=%s", "$opt_vardir/tmp/$name.result");
  mtr_add_arg($args, "--test-file=%s", "include/check-testcase.test");
  mtr_add_arg($args, "--verbose");

  if ( $mode eq "before" )
  {
    mtr_add_arg($args, "--record");
  }
  my $errfile= "$opt_vardir/tmp/$name.err";
  my $proc= My::SafeProcess->new
    (
     name          => $name,
     path          => $exe_mysqltest,
     error         => $errfile,
     output        => $errfile,
     args          => \$args,
     user_data     => $errfile,
     verbose       => $opt_verbose,
    );

  mtr_report("Started $proc");
  return $proc;
}


sub run_mysqltest ($) {
  my $proc= start_mysqltest(@_);
  $proc->wait();
}


sub start_mysqltest ($) {
  my ($tinfo)= @_;
  my $exe= $exe_mysqltest;
  my $args;

  mark_time_used('admin');

  mtr_init_args(\$args);

  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--silent");
  mtr_add_arg($args, "--tmpdir=%s", $opt_tmpdir);
  mtr_add_arg($args, "--character-sets-dir=%s", $path_charsetsdir);
  mtr_add_arg($args, "--logdir=%s/log", $opt_vardir);

  # Log line number and time  for each line in .test file
  mtr_add_arg($args, "--mark-progress")
    if $opt_mark_progress;

  mtr_add_arg($args, "--database=test");

  if ( $opt_ps_protocol )
  {
    mtr_add_arg($args, "--ps-protocol");
  }

  if ( $opt_sp_protocol )
  {
    mtr_add_arg($args, "--sp-protocol");
  }

  if ( $opt_view_protocol )
  {
    mtr_add_arg($args, "--view-protocol");
  }

  if ( $opt_cursor_protocol )
  {
    mtr_add_arg($args, "--cursor-protocol");
  }

  if ( $opt_non_blocking_api )
  {
    mtr_add_arg($args, "--non-blocking-api");
  }

  if ( $opt_strace_client )
  {
    $exe=  $opt_strace_client || "strace";
    mtr_add_arg($args, "-o");
    mtr_add_arg($args, "%s/log/mysqltest.strace", $opt_vardir);
    mtr_add_arg($args, "$exe_mysqltest");
  }

  mtr_add_arg($args, "--timer-file=%s/log/timer", $opt_vardir);

  if ( $opt_compress )
  {
    mtr_add_arg($args, "--compress");
  }

  if ( $opt_sleep )
  {
    mtr_add_arg($args, "--sleep=%d", $opt_sleep);
  }

  if ( $opt_ssl )
  {
    # Turn on SSL for _all_ test cases if option --ssl was used
    mtr_add_arg($args, "--ssl");
  }

  if ( $opt_max_connections ) {
    mtr_add_arg($args, "--max-connections=%d", $opt_max_connections);
  }

  if ( $opt_embedded_server )
  {

    # Get the args needed for the embedded server
    # and append them to args prefixed
    # with --sever-arg=

    my $mysqld=  $config->group('embedded')
      or mtr_error("Could not get [embedded] section");

    my $mysqld_args;
    mtr_init_args(\$mysqld_args);
    my $extra_opts= get_extra_opts($mysqld, $tinfo);
    mysqld_arguments($mysqld_args, $mysqld, $extra_opts);
    mtr_add_arg($args, "--server-arg=%s", $_) for @$mysqld_args;

    if (IS_WINDOWS)
    {
      # Trick the server to send output to stderr, with --console
      if (!(grep(/^--server-arg=--log-error/, @$args))) {
        mtr_add_arg($args, "--server-arg=--console");
      }
    }
  }

  # ----------------------------------------------------------------------
  # export MYSQL_TEST variable containing <path>/mysqltest <args>
  # ----------------------------------------------------------------------
  $ENV{'MYSQL_TEST'}= mtr_args2str($exe_mysqltest, @$args);

  # ----------------------------------------------------------------------
  # Add arguments that should not go into the MYSQL_TEST env var
  # ----------------------------------------------------------------------
  if ( $opt_valgrind_mysqltest )
  {
    # Prefix the Valgrind options to the argument list.
    # We do this here, since we do not want to Valgrind the nested invocations
    # of mysqltest; that would mess up the stderr output causing test failure.
    my @args_saved = @$args;
    mtr_init_args(\$args);
    valgrind_arguments($args, \$exe);
    mtr_add_arg($args, "%s", $_) for @args_saved;
  }

  if ($opt_force > 1)
  {
    mtr_add_arg($args, "--continue-on-error");
  }

  my $suite = $tinfo->{suite};
  if ($suite->{parent}) {
    mtr_add_arg($args, "--overlay-dir=%s/", $suite->{dir});
    mtr_add_arg($args, "--suite-dir=%s/", $suite->{parent}->{dir});
  } else {
    mtr_add_arg($args, "--suite-dir=%s/", $suite->{dir});
  }

  mtr_add_arg($args, "--test-file=%s", $tinfo->{'path'});

  # Number of lines of resut to include in failure report
  mtr_add_arg($args, "--tail-lines=20");

  if ( defined $tinfo->{'result_file'} ) {
    mtr_add_arg($args, "--result-file=%s", $tinfo->{'result_file'});
  }

  client_debug_arg($args, "mysqltest");

  if ( $opt_record )
  {
    mtr_add_arg($args, "--record");

    # When recording to a non existing result file
    # the name of that file is in "record_file"
    if ( defined $tinfo->{'record_file'} ) {
      mtr_add_arg($args, "--result-file=%s", $tinfo->{record_file});
    }
  }

  if ( $opt_client_gdb )
  {
    gdb_arguments(\$args, \$exe, "client");
  }
  elsif ( $opt_client_ddd )
  {
    ddd_arguments(\$args, \$exe, "client");
  }
  if ( $opt_client_dbx ) {
    dbx_arguments(\$args, \$exe, "client");
  }
  elsif ( $opt_client_debugger )
  {
    debugger_arguments(\$args, \$exe, "client");
  }

  my $proc= My::SafeProcess->new
    (
     name          => "mysqltest",
     path          => $exe,
     args          => \$args,
     append        => 1,
     error         => $path_current_testlog,
     verbose       => $opt_verbose,
    );
  mtr_verbose("Started $proc");
  return $proc;
}

#
# Modify the exe and args so that program is run in gdb in xterm
#
sub gdb_arguments {
  my $args= shift;
  my $exe=  shift;
  my $type= shift;
  my $input= shift;

  my $gdb_init_file= "$opt_vardir/tmp/gdbinit.$type";

  # Remove the old gdbinit file
  unlink($gdb_init_file);

  # Put $args into a single string
  my $str= join(" ", @$$args);
  $input = $input ? "< $input" : "";

  # write init file for mysqld or client
  mtr_tofile($gdb_init_file, "set args $str $input\n");

  if ( $opt_manual_gdb )
  {
     print "\nTo start gdb for $type, type in another window:\n";
     print "gdb -cd $glob_mysql_test_dir -x $gdb_init_file $$exe\n";

     # Indicate the exe should not be started
     $$exe= undef;
     return;
  }

  $$args= [];
  mtr_add_arg($$args, "-title");
  mtr_add_arg($$args, "$type");
  mtr_add_arg($$args, "-e");

  if ( $exe_libtool )
  {
    mtr_add_arg($$args, $exe_libtool);
    mtr_add_arg($$args, "--mode=execute");
  }

  mtr_add_arg($$args, "gdb");
  mtr_add_arg($$args, "-x");
  mtr_add_arg($$args, "$gdb_init_file");
  mtr_add_arg($$args, "$$exe");

  $$exe= "xterm";
}

#
# Modify the exe and args so that program is run in lldb
#
sub lldb_arguments {
  my $args= shift;
  my $exe= shift;
  my $type= shift;
  my $input= shift;

  my $lldb_init_file= "$opt_vardir/tmp/lldbinit.$type";
  unlink($lldb_init_file);

  # Put $args into a single string
  my $str= join(" ", @$$args);
  $input = $input ? "< $input" : "";

  # write init file for mysqld or client
  mtr_tofile($lldb_init_file, "set args $str $input\n");

    print "\nTo start lldb for $type, type in another window:\n";
    print "cd $glob_mysql_test_dir && lldb -s $lldb_init_file $$exe\n";

    # Indicate the exe should not be started
    $$exe= undef;
    return;
}

#
# Modify the exe and args so that program is run in ddd
#
sub ddd_arguments {
  my $args= shift;
  my $exe=  shift;
  my $type= shift;
  my $input= shift;

  my $gdb_init_file= "$opt_vardir/tmp/gdbinit.$type";

  # Remove the old gdbinit file
  unlink($gdb_init_file);

  # Put $args into a single string
  my $str= join(" ", @$$args);
  $input = $input ? "< $input" : "";

  # write init file for mysqld or client
  mtr_tofile($gdb_init_file, "file $$exe\nset args $str $input\n");

  if ( $opt_manual_ddd )
  {
     print "\nTo start ddd for $type, type in another window:\n";
     print "ddd -cd $glob_mysql_test_dir -x $gdb_init_file $$exe\n";

     # Indicate the exe should not be started
     $$exe= undef;
     return;
  }

  my $save_exe= $$exe;
  $$args= [];
  if ( $exe_libtool )
  {
    $$exe= $exe_libtool;
    mtr_add_arg($$args, "--mode=execute");
    mtr_add_arg($$args, "ddd");
  }
  else
  {
    $$exe= "ddd";
  }
  mtr_add_arg($$args, "--command=$gdb_init_file");
  mtr_add_arg($$args, "$save_exe");
}


#
# Modify the exe and args so that program is run in dbx in xterm
#
sub dbx_arguments {
  my $args= shift;
  my $exe=  shift;
  my $type= shift;
  my $input= shift;

  # Put $args into a single string
  my $str= join " ", @$$args;
  my $runline= $input ? "run $str < $input" : "run $str";

  if ( $opt_manual_dbx ) {
    print "\nTo start dbx for $type, type in another window:\n";
    print "cd $glob_mysql_test_dir; dbx -c \"stop in main; " .
          "$runline\" $$exe\n";

    # Indicate the exe should not be started
    $$exe= undef;
    return;
  }

  $$args= [];
  mtr_add_arg($$args, "-title");
  mtr_add_arg($$args, "$type");
  mtr_add_arg($$args, "-e");

  if ( $exe_libtool ) {
    mtr_add_arg($$args, $exe_libtool);
    mtr_add_arg($$args, "--mode=execute");
  }

  mtr_add_arg($$args, "dbx");
  mtr_add_arg($$args, "-c");
  mtr_add_arg($$args, "stop in main; $runline");
  mtr_add_arg($$args, "$$exe");

  $$exe= "xterm";
}


#
# Modify the exe and args so that program is run in the selected debugger
#
sub debugger_arguments {
  my $args= shift;
  my $exe=  shift;
  my $debugger= $opt_debugger || $opt_client_debugger;

  if ( $debugger =~ /vcexpress|vc|devenv/ )
  {
    # vc[express] /debugexe exe arg1 .. argn

    # Add name of the exe and /debugexe before args
    unshift(@$$args, "$$exe");
    unshift(@$$args, "/debugexe");

    # Set exe to debuggername
    $$exe= $debugger;

  }
  elsif ( $debugger =~ /windbg/ )
  {
    # windbg exe arg1 .. argn

    # Add name of the exe before args
    unshift(@$$args, "$$exe");

    # Set exe to debuggername
    $$exe= $debugger;

  }
  else
  {
    mtr_error("Unknown argument \"$debugger\" passed to --debugger");
  }
}


#
# Modify the exe and args so that program is run in valgrind
#
sub valgrind_arguments {
  my $args= shift;
  my $exe=  shift;

  if ( $opt_callgrind)
  {
    mtr_add_arg($args, "--tool=callgrind");
    mtr_add_arg($args, "--base=$opt_vardir/log");
  }
  else
  {
    mtr_add_arg($args, "--tool=memcheck"); # From >= 2.1.2 needs this option
    mtr_add_arg($args, "--leak-check=yes");
    mtr_add_arg($args, "--num-callers=16");
    mtr_add_arg($args, "--suppressions=%s/valgrind.supp", $glob_mysql_test_dir)
      if -f "$glob_mysql_test_dir/valgrind.supp";
  }

  # Add valgrind options, can be overriden by user
  mtr_add_arg($args, '%s', $_) for (@valgrind_args);

  mtr_add_arg($args, $$exe);

  $$exe= $opt_valgrind_path || "valgrind";

  if ($exe_libtool)
  {
    # Add "libtool --mode-execute" before the test to execute
    # if running in valgrind(to avoid valgrinding bash)
    unshift(@$args, "--mode=execute", $$exe);
    $$exe= $exe_libtool;
  }
}

#
# Modify the exe and args so that program is run in strace
#
sub strace_arguments {
  my $args= shift;
  my $exe=  shift;
  my $mysqld_name= shift;

  mtr_add_arg($args, "-f");
  mtr_add_arg($args, "-o%s/var/log/%s.strace", $glob_mysql_test_dir, $mysqld_name);

  # Add strace options, can be overriden by user
  mtr_add_arg($args, '%s', $_) for (@strace_args);

  mtr_add_arg($args, $$exe);

  $$exe= "strace";

  if ($exe_libtool)
  {
    # Add "libtool --mode-execute" before the test to execute
    # if running in valgrind(to avoid valgrinding bash)
    unshift(@$args, "--mode=execute", $$exe);
    $$exe= $exe_libtool;
  }
}

#
# Search server logs for valgrind reports printed at mysqld termination
#
sub valgrind_exit_reports() {
  my $found_err= 0;

  foreach my $log_file (keys %mysqld_logs)
  {
    my @culprits= ();
    my $valgrind_rep= "";
    my $found_report= 0;
    my $err_in_report= 0;

    my $LOGF = IO::File->new($log_file)
      or mtr_error("Could not open file '$log_file' for reading: $!");

    while ( my $line = <$LOGF> )
    {
      if ($line =~ /^CURRENT_TEST: (.+)$/)
      {
        my $testname= $1;
        # If we have a report, report it if needed and start new list of tests
        if ($found_report)
        {
          if ($err_in_report)
          {
            mtr_print ("Valgrind report from $log_file after tests:\n",
                        @culprits);
            mtr_print_line();
            print ("$valgrind_rep\n");
            $found_err= 1;
            $err_in_report= 0;
          }
          # Make ready to collect new report
          @culprits= ();
          $found_report= 0;
          $valgrind_rep= "";
        }
        push (@culprits, $testname);
        next;
      }
      # This line marks the start of a valgrind report
      $found_report= 1 if $line =~ /^==\d+== .* SUMMARY:/;

      if ($found_report) {
        $line=~ s/^==\d+== //;
        $valgrind_rep .= $line;
        $err_in_report= 1 if $line =~ /ERROR SUMMARY: [1-9]/;
        $err_in_report= 1 if $line =~ /definitely lost: [1-9]/;
        $err_in_report= 1 if $line =~ /possibly lost: [1-9]/;
      }
    }

    $LOGF= undef;

    if ($err_in_report) {
      mtr_print ("Valgrind report from $log_file after tests:\n", @culprits);
      mtr_print_line();
      print ("$valgrind_rep\n");
      $found_err= 1;
    }
  }

  return $found_err;
}

#
# Usage
#
sub usage ($) {
  my ($message)= @_;

  if ( $message )
  {
    print STDERR "$message\n";
    print STDERR "For full list of options, use $0 --help\n";
    exit;      
  }

  local $"= ','; # for @DEFAULT_SUITES below

  print <<HERE;

$0 [ OPTIONS ] [ TESTCASE ]

Options to control what engine/variation to run

  embedded-server       Use the embedded server, i.e. no mysqld daemons
  ps-protocol           Use the binary protocol between client and server
  cursor-protocol       Use the cursor protocol between client and server
                        (implies --ps-protocol)
  view-protocol         Create a view to execute all non updating queries
  sp-protocol           Create a stored procedure to execute all queries
  non-blocking-api      Use the non-blocking client API
  compress              Use the compressed protocol between client and server
  ssl                   Use ssl protocol between client and server
  skip-ssl              Dont start server with support for ssl connections
  vs-config             Visual Studio configuration used to create executables
                        (default: MTR_VS_CONFIG environment variable)
  parallel=#            How many parallell test should be run
  defaults-file=<config template> Use fixed config template for all
                        tests
  defaults-extra-file=<config template> Extra config template to add to
                        all generated configs
  combination=<opt>     Use at least twice to run tests with specified
                        options to mysqld

Options to control directories to use
  tmpdir=DIR            The directory where temporary files are stored
                        (default: ./var/tmp).
  vardir=DIR            The directory where files generated from the test run
                        is stored (default: ./var). Specifying a ramdisk or
                        tmpfs will speed up tests.
  mem                   Run testsuite in "memory" using tmpfs or ramdisk
                        Attempts to find a suitable location
                        using a builtin list of standard locations
                        for tmpfs (/run/shm, /dev/shm, /tmp)
                        The option can also be set using environment
                        variable MTR_MEM=[DIR]
  clean-vardir          Clean vardir if tests were successful and if
                        running in "memory". Otherwise this option is ignored
  client-bindir=PATH    Path to the directory where client binaries are located
  client-libdir=PATH    Path to the directory where client libraries are located


Options to control what test suites or cases to run

  force                 Continue after a failure. When specified once, a
                        failure in a test file will abort this test file, and
                        the execution will continue from the next test file.
                        When specified twice, execution will continue executing
                        the failed test file from the next command.
  with-ndbcluster-only  Run only tests that include "ndb" in the filename
  skip-ndb[cluster]     Skip all tests that need cluster. Default.
  include-ndb[cluster]  Enable all tests that need cluster
  do-test=PREFIX or REGEX
                        Run test cases which name are prefixed with PREFIX
                        or fulfills REGEX
  skip-test=PREFIX or REGEX
                        Skip test cases which name are prefixed with PREFIX
                        or fulfills REGEX
  start-from=PREFIX     Run test cases starting test prefixed with PREFIX where
                        prefix may be suite.testname or just testname
  suite[s]=NAME1,..,NAMEN
                        Collect tests in suites from the comma separated
                        list of suite names.
                        The default is: "@DEFAULT_SUITES"
  skip-rpl              Skip the replication test cases.
  big-test              Also run tests marked as "big". Repeat this option
                        twice to run only "big" tests.
  staging-run           Run a limited number of tests (no slow tests). Used
                        for running staging trees with valgrind.
  enable-disabled       Run also tests marked as disabled
  print-testcases       Don't run the tests but print details about all the
                        selected tests, in the order they would be run.
  skip-test-list=FILE   Skip the tests listed in FILE. Each line in the file
                        is an entry and should be formatted as: 
                        <TESTNAME> : <COMMENT>

Options that specify ports

  mtr-port-base=#       Base for port numbers, ports from this number to
  port-base=#           number+9 are reserved. Should be divisible by 10;
                        if not it will be rounded down. May be set with
                        environment variable MTR_PORT_BASE. If this value is
                        set and is not "auto", it overrides build-thread.
  mtr-build-thread=#    Specify unique number to calculate port number(s) from.
  build-thread=#        Can be set in environment variable MTR_BUILD_THREAD.
                        Set  MTR_BUILD_THREAD="auto" to automatically aquire
                        a build thread id that is unique to current host

Options for test case authoring

  record TESTNAME       (Re)genereate the result file for TESTNAME
  check-testcases       Check testcases for sideeffects
  mark-progress         Log line number and elapsed time to <testname>.progress

Options that pass on options (these may be repeated)

  mysqld=ARGS           Specify additional arguments to "mysqld"
  mysqld-env=VAR=VAL    Specify additional environment settings for "mysqld"

Options to run test on running server

  extern option=value   Run only the tests against an already started server
                        the options to use for connection to the extern server
                        must be specified using name-value pair notation
                        For example:
                         ./$0 --extern socket=/tmp/mysqld.sock

Options for debugging the product

  boot-dbx              Start bootstrap server in dbx
  boot-ddd              Start bootstrap server in ddd
  boot-gdb              Start bootstrap server in gdb
  client-dbx            Start mysqltest client in dbx
  client-ddd            Start mysqltest client in ddd
  client-debugger=NAME  Start mysqltest in the selected debugger
  client-gdb            Start mysqltest client in gdb
  dbx                   Start the mysqld(s) in dbx
  ddd                   Start the mysqld(s) in ddd
  debug                 Dump trace output for all servers and client programs
  debug-common          Same as debug, but sets 'd' debug flags to
                        "query,info,error,enter,exit"
  debug-server          Use debug version of server, but without turning on
                        tracing
  debugger=NAME         Start mysqld in the selected debugger
  gdb                   Start the mysqld(s) in gdb
  manual-debug          Let user manually start mysqld in debugger, before
                        running test(s)
  manual-gdb            Let user manually start mysqld in gdb, before running
                        test(s)
  manual-ddd            Let user manually start mysqld in ddd, before running
                        test(s)
  manual-dbx            Let user manually start mysqld in dbx, before running
                        test(s)
  manual-lldb           Let user manually start mysqld in lldb, before running 
                        test(s)
  max-save-core         Limit the number of core files saved (to avoid filling
                        up disks for heavily crashing server). Defaults to
                        $opt_max_save_core, set to 0 for no limit. Set
                        it's default with MTR_MAX_SAVE_CORE
  max-save-datadir      Limit the number of datadir saved (to avoid filling
                        up disks for heavily crashing server). Defaults to
                        $opt_max_save_datadir, set to 0 for no limit. Set
                        it's default with MTR_MAX_SAVE_DATADIR
  max-test-fail         Limit the number of test failurs before aborting
                        the current test run. Defaults to
                        $opt_max_test_fail, set to 0 for no limit. Set
                        it's default with MTR_MAX_TEST_FAIL

Options for valgrind

  valgrind              Run the "mysqltest" and "mysqld" executables using
                        valgrind with default options
  valgrind-all          Synonym for --valgrind
  valgrind-mysqltest    Run the "mysqltest" and "mysql_client_test" executable
                        with valgrind
  valgrind-mysqld       Run the "mysqld" executable with valgrind
  valgrind-options=ARGS Deprecated, use --valgrind-option
  valgrind-option=ARGS  Option to give valgrind, replaces default option(s),
                        can be specified more then once
  valgrind-path=<EXE>   Path to the valgrind executable
  callgrind             Instruct valgrind to use callgrind

Options for strace

  strace                Run the "mysqld" executables using strace. Default
                        options are -f -o var/log/'mysqld-name'.strace
  strace-option=ARGS    Option to give strace, replaces default option(s),
  strace-client=[path]  Create strace output for mysqltest client, optionally
                        specifying name and path to the trace program to use.
                        Example: $0 --strace-client=ktrace

Misc options
  user=USER             User for connecting to mysqld(default: $opt_user)
  comment=STR           Write STR to the output
  timer                 Show test case execution time.
  verbose               More verbose output(use multiple times for even more)
  verbose-restart       Write when and why servers are restarted
  start                 Only initialize and start the servers, using the
                        startup settings for the first specified test case
                        Example:
                         $0 --start alias &
  start-and-exit        Same as --start, but mysql-test-run terminates and
                        leaves just the server running
  start-dirty           Only start the servers (without initialization) for
                        the first specified test case
  user-args             In combination with start* and no test name, drops
                        arguments to mysqld except those speficied with
                        --mysqld (if any)
  wait-all              If --start or --start-dirty option is used, wait for all
                        servers to exit before finishing the process
  fast                  Run as fast as possible, dont't wait for servers
                        to shutdown etc.
  force-restart         Always restart servers between tests
  parallel=N            Run tests in N parallel threads (default 1)
                        Use parallel=auto for auto-setting of N
  repeat=N              Run each test N number of times
  retry=N               Retry tests that fail up to N times (default $opt_retry).
                        Retries are also limited by the maximum number of
                        failures before stopping, set with the --retry-failure
                        option
  retry-failure=N       When using the --retry option to retry failed tests,
                        stop when N failures have occured (default $opt_retry_failure)
  reorder               Reorder tests to get fewer server restarts
  help                  Get this help text

  testcase-timeout=MINUTES Max test case run time (default $opt_testcase_timeout)
  suite-timeout=MINUTES Max test suite run time (default $opt_suite_timeout)
  shutdown-timeout=SECONDS Max number of seconds to wait for server shutdown
                        before killing servers (default $opt_shutdown_timeout)
  warnings              Scan the log files for warnings. Use --nowarnings
                        to turn off.

  stop-file=file        (also MTR_STOP_FILE environment variable) if this
                        file detected mysql test will not start new tests
                        until the file will be removed.
  stop-keep-alive=sec   (also MTR_STOP_KEEP_ALIVE environment variable)
                        works with stop-file, print messages every sec
                        seconds when mysql test is waiting to removing
                        the file (for buildbot)

  sleep=SECONDS         Passed to mysqltest, will be used as fixed sleep time
  debug-sync-timeout=NUM Set default timeout for WAIT_FOR debug sync
                        actions. Disable facility with NUM=0.
  gcov                  Collect coverage information after the test.
                        The result is a gcov file per source and header file.
  gcov-src-dir=subdir   Colllect coverage only within the given subdirectory.
                        For example, if you're only developing the SQL layer, 
                        it makes sense to use --gcov-src-dir=sql
  gprof                 Collect profiling information using gprof.
  experimental=<file>   Refer to list of tests considered experimental;
                        failures will be marked exp-fail instead of fail.
  timestamp             Print timestamp before each test report line
  timediff              With --timestamp, also print time passed since
                        *previous* test started
  max-connections=N     Max number of open connection to server in mysqltest
  default-myisam        Set default storage engine to MyISAM for non-innodb
                        tests. This is needed after switching default storage
                        engine to InnoDB.
  report-times          Report how much time has been spent on different
                        phases of test execution.
  stress=ARGS           Run stress test, providing options to
                        mysql-stress-test.pl. Options are separated by comma.

Some options that control enabling a feature for normal test runs,
can be turned off by prepending 'no' to the option, e.g. --notimer.
This applies to reorder, timer, check-testcases and warnings.

HERE
  exit(1);

}

sub list_options ($) {
  my $hash= shift;

  for (keys %$hash) {
    s/([:=].*|[+!])$//;
    s/\|/\n--/g;
    print "--$_\n";
  }

  exit(1);
}

sub time_format($) {
  sprintf '%d:%02d:%02d', $_[0]/3600, ($_[0]/60)%60, $_[0]%60;
}

our $num_tests;

sub xterm_stat {
  if (-t STDOUT and defined $ENV{TERM} and $ENV{TERM} =~ /xterm/) {
    my ($left) = @_;

    # 2.5 -> best by test
    $num_tests = $left + 2.5 unless $num_tests;

    my $done = $num_tests - $left;
    my $spent = time - $^T;

    printf "\e];mtr: spent %s on %d tests. %s (%d tests) left\a",
           time_format($spent), $done,
           time_format($spent/$done * $left), $left;
  }
}
