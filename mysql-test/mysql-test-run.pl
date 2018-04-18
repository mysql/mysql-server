#!/usr/bin/perl
# -*- cperl -*-

# Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

##############################################################################
#
#  mysql-test-run.pl
#
#  Tool used for executing a suite of .test files
#
#  See the "MySQL Test framework manual" for more information
#  https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_MYSQL_TEST_RUN.html
#
##############################################################################

use strict;
use warnings;

use lib "lib";

use Cwd;
use Cwd 'abs_path';
use File::Basename;
use File::Copy;
use File::Find;
use File::Spec::Functions qw/splitdir/;
use File::Temp qw/tempdir/;
use Getopt::Long;
use IO::Select;
use IO::Socket::INET;

push @INC, ".";

use My::ConfigFactory;
use My::CoreDump;
use My::File::Path;    # Patched version of File::Path
use My::Find;
use My::Options;
use My::Platform;
use My::SafeProcess;
use My::SysInfo;

use mtr_cases;
use mtr_cases_from_list;
use mtr_match;
use mtr_report;
use mtr_results;
use mtr_unique;

require "lib/mtr_gcov.pl";
require "lib/mtr_gprof.pl";
require "lib/mtr_io.pl";
require "lib/mtr_misc.pl";
require "lib/mtr_process.pl";

$SIG{INT} = sub { mtr_error("Got ^C signal"); };

sub env_or_val($$) { defined $ENV{ $_[0] } ? $ENV{ $_[0] } : $_[1] }

# Local variables
my $opt_boot_dbx;
my $opt_boot_ddd;
my $opt_boot_gdb;
my $opt_callgrind;
my $opt_charset_for_testdb;
my $opt_compress;
my $opt_cursor_protocol;
my $opt_debug_common;
my $opt_do_suite;
my $opt_explain_protocol;
my $opt_helgrind;
my $opt_json_explain_protocol;
my $opt_mark_progress;
my $opt_max_connections;
my $opt_ps_protocol;
my $opt_report_features;
my $opt_skip_core;
my $opt_skip_ssl;
my $opt_skip_test_list;
my $opt_sleep;
my $opt_sp_protocol;
my $opt_start;
my $opt_start_dirty;
my $opt_start_exit;
my $opt_strace_client;
my $opt_strace_server;
my $opt_stress;
my $opt_suites;
my $opt_tmpdir;
my $opt_tmpdir_pid;
my $opt_trace_protocol;
my $opt_user_args;
my $opt_valgrind_path;
my $opt_view_protocol;
my $opt_wait_all;

my $opt_build_thread  = $ENV{'MTR_BUILD_THREAD'}  || "auto";
my $opt_colored_diff  = $ENV{'MTR_COLORED_DIFF'}  || 0;
my $opt_ctest_timeout = $ENV{'MTR_CTEST_TIMEOUT'} || 120;      # seconds
my $opt_debug_sync_timeout = 600;    # Default timeout for WAIT_FOR actions.
my $opt_do_test_list       = "";
my $opt_force_restart      = 0;
my $opt_include_ndbcluster = 0;
my $opt_max_save_core      = env_or_val(MTR_MAX_SAVE_CORE => 5);
my $opt_max_save_datadir   = env_or_val(MTR_MAX_SAVE_DATADIR => 20);
my $opt_max_test_fail      = env_or_val(MTR_MAX_TEST_FAIL => 10);
my $opt_mysqlx_baseport    = $ENV{'MYSQLXPLUGIN_PORT'} || "auto";
my $opt_port_base          = $ENV{'MTR_PORT_BASE'} || "auto";
my $opt_reorder            = 1;
my $opt_retry              = 3;
my $opt_retry_failure      = env_or_val(MTR_RETRY_FAILURE => 2);
my $opt_shutdown_timeout   = $ENV{MTR_SHUTDOWN_TIMEOUT} || 10;         # seconds
my $opt_skip_ndbcluster    = 0;
my $opt_skip_sys_schema    = 0;
my $opt_start_timeout      = $ENV{MTR_START_TIMEOUT} || 180;           # seconds
my $opt_suite_timeout      = $ENV{MTR_SUITE_TIMEOUT} || 300;           # minutes
my $opt_testcase_timeout   = $ENV{MTR_TESTCASE_TIMEOUT} || 15;         # minutes
my $opt_valgrind_clients   = 0;
my $opt_valgrind_mysqld    = 0;
my $opt_valgrind_mysqltest = 0;

# Options used when connecting to an already running server
my %opts_extern;

my $auth_plugin;            # The path to the authentication test plugin
my $baseport;
my $config;                 # The currently running config
my $ctest_report;           # Unit test report stored here for delayed printing
my $current_config_name;    # The currently running config file template
my $exe_ndb_mgm;
my $exe_ndb_mgmd;
my $exe_ndb_waiter;
my $exe_ndbd;
my $exe_ndbmtd;
my $initial_bootstrap_cmd;
my $mysql_base_version;
my $mysqlx_baseport;
my $path_config_file;       # The generated config file, var/my.cnf
my $path_vardir_trace;      # Unix formatted opt_vardir for trace files

my $DEFAULT_SUITES =
"main,sys_vars,binlog,binlog_gtid,binlog_nogtid,federated,gis,rpl,rpl_gtid,rpl_nogtid,innodb,innodb_gis,innodb_fts,innodb_zip,innodb_undo,perfschema,funcs_1,opt_trace,parts,auth_sec,query_rewrite_plugins,gcol,sysschema,test_service_sql_api,json,connection_control,test_services,collations,service_udf_registration,service_sys_var_registration,service_status_var_registration,x";

my $build_thread       = 0;
my $daemonize_mysqld   = 0;
my $debug_d            = "d";
my $exe_ndbmtd_counter = 0;
my $ports_per_thread   = 20;
my $source_dist        = 0;
my $valgrind_reports   = 0;

my @valgrind_args;

my %mysqld_logs;

# Storage for changed environment variables
my %old_env;

# Global variables
our $opt_client_dbx;
our $opt_client_ddd;
our $opt_client_debugger;
our $opt_client_gdb;
our $opt_client_lldb;
our $opt_ctest_report;
our $opt_dbx;
our $opt_ddd;
our $opt_debug;
our $opt_debug_server;
our $opt_debugger;
our $opt_force;
our $opt_gcov;
our $opt_gdb;
our $opt_gprof;
our $opt_lldb;
our $opt_manual_boot_gdb;
our $opt_manual_dbx;
our $opt_manual_ddd;
our $opt_manual_debug;
our $opt_manual_gdb;
our $opt_manual_lldb;
our $opt_no_skip;
our $opt_non_parallel_test;
our $opt_record;
our $opt_report_unstable_tests;
our $opt_ssl;
our $opt_ssl_supported;
our $opt_suite_opt;
our $opt_summary_report;
our $opt_test_progress;
our $opt_vardir;
our $opt_xml_report;

our $opt_big_test        = 0;
our $opt_check_testcases = 1;
our $opt_clean_vardir    = $ENV{'MTR_CLEAN_VARDIR'};
our $opt_ctest           = env_or_val(MTR_UNIT_TESTS => -1);
our $opt_fast            = 0;
our $opt_gcov_err        = "mysql-test-gcov.err";
our $opt_gcov_exe        = "gcov";
our $opt_gcov_msg        = "mysql-test-gcov.msg";
our $opt_mem             = $ENV{'MTR_MEM'} ? 1 : 0;
our $opt_only_big_test   = 0;
our $opt_parallel        = $ENV{MTR_PARALLEL};
our $opt_repeat          = 1;
our $opt_report_times    = 0;
our $opt_resfile         = $ENV{'MTR_RESULT_FILE'} || 0;
our $opt_sanitize        = 0;
our $opt_user            = "root";
our $opt_valgrind        = 0;
our $opt_verbose         = 0;
# Visual Studio produces executables in different sub-directories
# based on the configuration used to build them. To make life easier,
# an environment variable or command-line option may be specified to
# control which set of executables will be used by the test suite.
our $opt_vs_config = $ENV{'MTR_VS_CONFIG'};
our $opt_warnings  = 1;

our @opt_cases;
our @opt_combinations;
our @opt_experimentals;
our @opt_extra_bootstrap_opt;
our @opt_extra_mysqld_opt;
our @opt_extra_mysqltest_opt;
our @opt_mysqld_envs;

our $basedir;
our $bindir;
our $build_thread_id_dir;
our $build_thread_id_file;
our $debug_compiled_binaries;
our $default_vardir;
our $excluded_string;
our $exe_libtool;
our $exe_mysql;
our $exe_mysql_ssl_rsa_setup;
our $exe_mysqladmin;
our $exe_mysqltest;
our $glob_mysql_test_dir;
our $mysql_version_extra;
our $mysql_version_id;
our $num_tests_for_report;    # for test-progress option
our $path_charsetsdir;
our $path_client_bindir;
our $path_client_libdir;
our $path_current_testlog;
our $path_language;
our $path_testlog;
our $remaining;
our $start_only;

our $experimental_test_cases = [];
our $glob_debugger           = 0;
our $group_replication       = 0;
our $ndbcluster_enabled      = 0;

our @share_locations;

our %gprof_dirs;
our %mysqld_variables;

sub check_timeout ($) { return testcase_timeout($_[0]) / 10; }

sub suite_timeout { return $opt_suite_timeout * 60; }

sub using_extern { return (keys %opts_extern > 0); }

# Return list of specific servers
sub _like { return $config ? $config->like($_[0]) : (); }

sub mysqlds     { return _like('mysqld.'); }
sub ndbds       { return _like('cluster_config.ndbd.'); }
sub ndb_mgmds   { return _like('cluster_config.ndb_mgmd.'); }
sub clusters    { return _like('mysql_cluster.'); }
sub memcacheds  { return _like('memcached.'); }
sub all_servers { return (mysqlds(), ndb_mgmds(), ndbds(), memcacheds()); }

# Return an object which refers to the group named '[mysqld]'
# from the my.cnf file. Options specified in the section can
# be accessed using it.
sub mysqld_group { return $config ? $config->group('mysqld') : (); }

sub testcase_timeout ($) {
  my ($tinfo) = @_;
  if (exists $tinfo->{'case-timeout'}) {
    # Return test specific timeout if *longer* that the general timeout
    my $test_to = $tinfo->{'case-timeout'};
    $test_to *= 10 if $opt_valgrind;
    return $test_to * 60 if $test_to > $opt_testcase_timeout;
  }
  return $opt_testcase_timeout * 60;
}

BEGIN {
  # Check that mysql-test-run.pl is started from mysql-test/
  unless (-f "mysql-test-run.pl") {
    print "**** ERROR **** ",
      "You must start mysql-test-run from the mysql-test/ directory\n";
    exit(1);
  }

  # Check that lib exist
  unless (-d "lib/") {
    print "**** ERROR **** ", "Could not find the lib/ directory \n";
    exit(1);
  }
}

END {
  if (defined $opt_tmpdir_pid and $opt_tmpdir_pid == $$) {
    if (!$opt_start_exit) {
      # Remove the tempdir this process has created
      mtr_verbose("Removing tmpdir $opt_tmpdir");
      rmtree($opt_tmpdir);
    } else {
      mtr_warning(
          "tmpdir $opt_tmpdir should be removed after the server has finished");
    }
  }
}

select(STDOUT);
$| = 1;    # Automatically flush STDOUT

main();

sub main {
  # Default, verbosity on
  report_option('verbose', 0);

  # This is needed for test log evaluation in "gen-build-status-page"
  # in all cases where the calling tool does not log the commands directly
  # before it executes them, like "make test-force-pl" in RPM builds.
  mtr_report("Logging: $0 ", join(" ", @ARGV));

  command_line_setup();

  # Create build thread id directory
  create_unique_id_dir();

  $build_thread_id_file = "$build_thread_id_dir/" . $$ . "_unique_ids.log";
  open(FH, ">>", $build_thread_id_file) or
    die "Can't open file $build_thread_id_file: $!";
  print FH "# Unique id file paths\n";
  close(FH);

  # --help will not reach here, so now it's safe to assume we have binaries
  My::SafeProcess::find_bin();

  if ($opt_gcov) {
    gcov_prepare($basedir);
  }

  # Collect test cases from a file and put them into '@opt_cases'.
  if ($opt_do_test_list) {
    collect_test_cases_from_list(\@opt_cases, $opt_do_test_list, \$opt_ctest);
  }

  if (!$opt_suites) {
    $opt_suites = $DEFAULT_SUITES;
  }

  if ($opt_skip_sys_schema) {
    $opt_suites =~ s/,sysschema//;
  }

  my $mtr_suites = $opt_suites;
  # Skip the suites that doesn't match --do-suite filter
  if ($opt_do_suite) {
    my $opt_do_suite_reg = init_pattern($opt_do_suite, "--do-suite");
    for my $suite (split(",", $opt_suites)) {
      if ($opt_do_suite_reg and not $suite =~ /$opt_do_suite_reg/) {
        $opt_suites =~ s/$suite,?//;
      }
    }

    # Removing ',' at the end of $opt_suites if exists
    $opt_suites =~ s/,$//;
  }

  if ($opt_suites) {
    mtr_report("Using suites: $opt_suites") unless @opt_cases;
  } else {
    if ($opt_do_suite) {
      mtr_error("The PREFIX/REGEX '$opt_do_suite' doesn't match any of " .
                "'$mtr_suites' suite(s)");
    }
  }

  # Environment variable to hold number of CPUs
  my $sys_info = My::SysInfo->new();
  $ENV{NUMBER_OF_CPUS} = $sys_info->num_cpus();

  if ($opt_parallel eq "auto") {
    # Try to find a suitable value for number of workers
    $opt_parallel = $ENV{NUMBER_OF_CPUS};
    if (defined $ENV{MTR_MAX_PARALLEL}) {
      my $max_par = $ENV{MTR_MAX_PARALLEL};
      $opt_parallel = $max_par if ($opt_parallel > $max_par);
    }
    $opt_parallel = 1 if ($opt_parallel < 1);
  }

  init_timers();

  mtr_report("Collecting tests...");
  my $tests = collect_test_cases($opt_reorder, $opt_suites,
                                 \@opt_cases,  $opt_skip_test_list);
  mark_time_used('collect');

  if ($opt_report_features) {
    # Put "report features" as the first test to run. No result file,
    # prints the output on console.
    my $tinfo = My::Test->new(master_opt    => [],
                              name          => 'report_features',
                              path          => 'include/report-features.test',
                              shortname     => 'report_features',
                              slave_opt     => [],
                              template_path => "include/default_my.cnf",);
    unshift(@$tests, $tinfo);
  }

  initialize_servers();

  my $num_tests = @$tests;
  $num_tests_for_report = $num_tests * $opt_repeat;
  $remaining            = $num_tests_for_report;

  # Limit parallel workers to number of tests to avoid idle workers
  $opt_parallel = $num_tests if ($num_tests > 0 and $opt_parallel > $num_tests);
  $ENV{MTR_PARALLEL} = $opt_parallel;
  mtr_report("Using parallel: $opt_parallel");

  my $is_option_mysqlx_port_set = $opt_mysqlx_baseport ne "auto";
  if ($opt_parallel > 1) {
    if ($opt_start_exit || $opt_stress || $is_option_mysqlx_port_set) {
      mtr_warning("Parallel cannot be used neither with --start-and-exit nor",
                  "--stress nor --mysqlx_port.\nSetting parallel value to 1.");
      $opt_parallel = 1;
    }
  }

  # Please note, that disk_usage() will print a space to separate its
  # information from the preceding string, if the disk usage report is
  # enabled. Otherwise an empty string is returned.
  my $disk_usage = disk_usage();
  if ($disk_usage) {
    mtr_report(sprintf("Disk usage of vardir in MB:%s", $disk_usage));
  }

  # Create server socket on any free port
  my $server = new IO::Socket::INET(Listen    => $opt_parallel,
                                    LocalAddr => 'localhost',
                                    Proto     => 'tcp',);
  mtr_error("Could not create testcase server port: $!") unless $server;
  my $server_port = $server->sockport();

  if ($opt_resfile) {
    resfile_init("$opt_vardir/mtr-results.txt");
    print_global_resfile();
  }

  if ($opt_summary_report) {
    mtr_summary_file_init($opt_summary_report);
  }
  if ($opt_xml_report) {
    mtr_xml_init($opt_xml_report);
  }

  # Read definitions from include/plugin.defs
  read_plugin_defs("include/plugin.defs");

  # Also read from any plugin local or suite specific plugin.defs
  my $plugin_def =
    "$basedir/plugin/*/tests/mtr/plugin.defs " .
    "$basedir/internal/plugin/*/tests/mtr/plugin.defs " .
    "$basedir/rapid/plugin/*/tests/mtr/plugin.defs " .
    "$basedir/components/*/tests/mtr/plugin.defs " . "suite/*/plugin.defs";

  for (glob $plugin_def) {
    read_plugin_defs($_);
  }

  # Simplify reference to semisync plugins
  $ENV{'SEMISYNC_PLUGIN_OPT'} = $ENV{'SEMISYNC_MASTER_PLUGIN_OPT'};

  if (IS_WINDOWS) {
    $ENV{'PLUGIN_SUFFIX'} = "dll";
  } else {
    $ENV{'PLUGIN_SUFFIX'} = "so";
  }

  if ($group_replication) {
    $ports_per_thread = $ports_per_thread + 10;
  }

  # Create child processes
  my %children;
  for my $child_num (1 .. $opt_parallel) {
    my $child_pid = My::SafeProcess::Base::_safe_fork();
    if ($child_pid == 0) {
      $server = undef;    # Close the server port in child
      $tests  = {};       # Don't need the tests list in child

      # Use subdir of var and tmp unless only one worker
      if ($opt_parallel > 1) {
        set_vardir("$opt_vardir/$child_num");
        $opt_tmpdir = "$opt_tmpdir/$child_num";
      }

      init_timers();
      run_worker($server_port, $child_num);
      exit(1);
    }

    $children{$child_pid} = 1;
  }

  mtr_report();
  mtr_print_thick_line();
  mtr_print_header($opt_parallel > 1);

  mark_time_used('init');

  my $completed = run_test_server($server, $tests, $opt_parallel);

  exit(0) if $opt_start_exit;

  # Send Ctrl-C to any children still running
  kill("INT", keys(%children));

  if (!IS_WINDOWS) {
    # Wait for children to exit
    foreach my $pid (keys %children) {
      my $ret_pid = waitpid($pid, 0);
      if ($ret_pid != $pid) {
        mtr_report("Unknown process $ret_pid exited");
      } else {
        delete $children{$ret_pid};
      }
    }
  }

  if (not $completed) {
    mtr_error("Test suite aborted");
  }

  if (@$completed != $num_tests) {
    # Not all tests completed, failure
    mtr_report();
    mtr_report("Only ", int(@$completed), " of $num_tests completed.");
    mtr_error("Not all tests completed");
  }

  mark_time_used('init');

  push @$completed, run_ctest() if $opt_ctest;

  if ($opt_valgrind_mysqld or $opt_sanitize) {
    # Create minimalistic "test" for the reporting
    my $tinfo = My::Test->new(
      name      => $opt_valgrind_mysqld ? 'valgrind_report' : 'sanitize_report',
      shortname => $opt_valgrind_mysqld ? 'valgrind_report' : 'sanitize_report',
    );

    # Set dummy worker id to align report with normal tests
    $tinfo->{worker} = 0 if $opt_parallel > 1;
    if ($valgrind_reports) {
      $tinfo->{result} = 'MTR_RES_FAILED';
      if ($opt_valgrind_mysqld) {
        $tinfo->{comment} = "Valgrind reported failures at shutdown, see above";
      } else {
        $tinfo->{comment} =
          "Sanitizer reported failures at shutdown, see above";
      }
      $tinfo->{failures} = 1;
    } else {
      $tinfo->{result} = 'MTR_RES_PASSED';
    }
    mtr_report_test($tinfo);
    push @$completed, $tinfo;
  }

  mtr_print_line();

  if ($opt_gcov) {
    gcov_collect($bindir, $opt_gcov_exe, $opt_gcov_msg, $opt_gcov_err);
  }

  if ($ctest_report) {
    print "$ctest_report\n";
    mtr_print_line();
  }

  # Cleanup the build thread id files
  remove_redundant_thread_id_file_locations();
  clean_unique_id_dir();

  print_total_times($opt_parallel) if $opt_report_times;

  mtr_report_stats("Completed", $completed);

  remove_vardir_subs() if $opt_clean_vardir;

  exit(0);
}

# The word server here refers to the main control loop of MTR, not a
# mysqld server. Worker threads have already been started when this sub
# is called. The main loop wakes up once every second and checks for new
# messages from the workers. After some special handling of new/closed
# connections, the bulk of the loop is handling the different messages.
#
# The message starts with a codeword, which can be 'TESTRESULT',
# 'START', 'SPENT' or 'VALGREP'.
#
# After 'TESTRESULT' or 'START', the master thread finds the next test
# to run by this worker. It also contains the logic to find a more
# optimal ordering of tests per worker in order to reduce number of
# restarts. When a test has been identified, it's sent to the worker
# with a message tagged 'TESTCASE'.
#
# When all tests are completed or if we abort test runs early, a message
# 'BYE' is sent to the worker.
sub run_test_server ($$$) {
  my ($server, $tests, $childs) = @_;

  my $num_failed_test   = 0; # Number of tests failed so far
  my $num_saved_cores   = 0; # Number of core files saved in vardir/log/ so far.
  my $num_saved_datadir = 0; # Number of datadirs saved in vardir/log/ so far.

  # Used as hint to CoreDump
  my $exe_mysqld = find_mysqld($basedir) || "";

  # Scheduler variables
  my $max_ndb = $ENV{MTR_MAX_NDB} || $childs / 2;
  $max_ndb = $childs if $max_ndb > $childs;
  $max_ndb = 1       if $max_ndb < 1;
  my $num_ndb_tests = 0;

  my $completed = [];
  my %running;
  my $result;

  my $completed_wid_count = 0;
  my $non_parallel_tests  = [];

  my $suite_timeout = start_timer(suite_timeout());

  my $s = IO::Select->new();
  $s->add($server);

  while (1) {
    mark_time_used('admin');
    # # Wake up once every second
    my @ready = $s->can_read(1);
    mark_time_idle();

    foreach my $sock (@ready) {
      if ($sock == $server) {
        # New client connected
        my $child = $sock->accept();
        mtr_verbose("Client connected");
        $s->add($child);
        print $child "HELLO\n";
      } else {
        my $line = <$sock>;
        if (!defined $line) {
          # Client disconnected
          mtr_verbose("Child closed socket");
          $s->remove($sock);
          if (--$childs == 0) {
            return $completed;
          }
          next;
        }
        chomp($line);

        if ($line eq 'TESTRESULT') {
          $result = My::Test::read_test($sock);

          # Report test status
          mtr_report_test($result);

          if ($result->is_failed()) {
            # Save the workers "savedir" in var/log
            my $worker_savedir  = $result->{savedir};
            my $worker_savename = basename($worker_savedir);
            my $savedir         = "$opt_vardir/log/$worker_savename";

            if ($opt_max_save_datadir > 0 &&
                $num_saved_datadir >= $opt_max_save_datadir) {
              mtr_report(" - skipping '$worker_savedir/'");
              rmtree($worker_savedir);
            } else {
              rename($worker_savedir, $savedir) if $worker_savedir ne $savedir;

              # Look for the test log file and put that in savedir location
              my $logfile     = "$result->{shortname}" . ".log";
              my $logfilepath = dirname($worker_savedir) . "/" . $logfile;
              move($logfilepath, $savedir);

              if ($opt_check_testcases &&
                  !defined $result->{'result_file'} &&
                  !$result->{timeout}) {
                mtr_report("Mysqltest client output from logfile");
                mtr_report("----------- MYSQLTEST OUTPUT START -----------\n");
                mtr_printfile($savedir . "/" . $logfile);
                mtr_report("\n------------ MYSQLTEST OUTPUT END -----------\n");
              }

              mtr_report(" - the logfile can be found in '$savedir/$logfile'");

              # Move any core files from e.g. mysqltest
              foreach my $coref (glob("core*"), glob("*.dmp")) {
                mtr_report(" - found '$coref', moving it to '$savedir'");
                move($coref, $savedir);
              }

              if ($opt_max_save_core > 0) {
                # Limit number of core files saved
                find(
                  { no_chdir => 1,
                    wanted   => sub {
                      my $core_file = $File::Find::name;
                      my $core_name = basename($core_file);

                      # Name beginning with core, not ending in .gz
                      if (($core_name =~ /^core/ and $core_name !~ /\.gz$/) or
                          (IS_WINDOWS and $core_name =~ /\.dmp$/)) {
                        # Ending with .dmp
                        mtr_report(" - found '$core_name'",
                                   "($num_saved_cores/$opt_max_save_core)");

                        My::CoreDump->show($core_file, $exe_mysqld,
                                           $opt_parallel);

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
            $num_failed_test++
              unless ($result->{retries} ||
                      $result->{exp_fail});

            if (!$opt_force) {
              # Test has failed, force is off
              push(@$completed, $result);
              return $completed unless $result->{'dont_kill_server'};
              # Prevent kill of server, to get valgrind report
              print $sock "BYE\n";
              next;
            } elsif ($opt_max_test_fail > 0 and
                     $num_failed_test >= $opt_max_test_fail) {
              push(@$completed, $result);
              mtr_report_stats("Too many failed", $completed, 1);
              mtr_report("Too many tests($num_failed_test) failed!",
                         "Terminating...");
              return undef;
            }
          }

          resfile_print_test();

          # Retry test run after test failure
          my $retries         = $result->{retries}  || 2;
          my $test_has_failed = $result->{failures} || 0;

          if ($test_has_failed and $retries <= $opt_retry) {
            # Test should be run one more time unless it has failed
            # too many times already
            my $tname    = $result->{name};
            my $failures = $result->{failures};

            if ($opt_retry > 1 and $failures >= $opt_retry_failure) {
              mtr_report("\nTest $tname has failed $failures times,",
                         "no more retries!\n");
            } else {
              mtr_report("\nRetrying test $tname, " .
                         "attempt($retries/$opt_retry)...\n");
              delete($result->{result});
              $result->{retries} = $retries + 1;
              $result->write_test($sock, 'TESTCASE');
              next;
            }
          }

          # Tests are already duplicated in the list if parallel value is
          # greater than 1. Following code is needed only when parallel
          # value is 1.
          if ($opt_parallel == 1) {
            # Repeat the test $opt_repeat number of times
            my $repeat = $result->{repeat} || 1;

            # Don't repeat if test was skipped
            if ($repeat < $opt_repeat &&
                $result->{'result'} ne 'MTR_RES_SKIPPED') {
              $result->{retries} = 0;
              $result->{rep_failures}++ if $result->{failures};
              $result->{failures} = 0;
              delete($result->{result});
              $result->{repeat} = $repeat + 1;
              $result->write_test($sock, 'TESTCASE');
              next;
            }
          }

          # Remove from list of running
          mtr_error("'", $result->{name}, "' is not known to be running")
            unless delete $running{ $result->key() };

          # Update scheduler variables
          $num_ndb_tests-- if ($result->{ndb_test});

          # Save result in completed list
          push(@$completed, $result);

        } elsif ($line eq 'START') {
          # 'START' is the initial message from a new worker, send first test
          ;
        } elsif ($line =~ /^SPENT/) {
          # 'SPENT' comes with numbers for how much time the worker spent in
          # various phases of execution, to be used when 'report-times' option
          # is enabled.
          add_total_times($line);
        } elsif ($line eq 'VALGREP' && ($opt_valgrind or $opt_sanitize)) {
          # 'VALGREP' means that the worker found some valgrind reports in the
          # server logs. This will cause the master to flag the pseudo test
          # valgrind_report as failed.
          $valgrind_reports = 1;
        } else {
          # Unknown message from worker
          mtr_error("Unknown response: '$line' from client");
        }

        # Find next test to schedule
        # - Try to use same configuration as worker used last time
        # - Limit number of parallel ndb tests
        my $next;
        my $second_best;

        for (my $i = 0 ; $i <= @$tests ; $i++) {
          my $t = $tests->[$i];
          last unless defined $t;

          if (run_testcase_check_skip_test($t)) {
            # Move the test to completed list
            push(@$completed, splice(@$tests, $i, 1));

            # Since the test at pos $i was taken away, next
            # test will also be at $i -> redo
            redo;
          }

          # Create a separate list for tests sourcing 'not_parallel.inc'
          # include file.
          if ($t->{'not_parallel'}) {
            push(@$non_parallel_tests, splice(@$tests, $i, 1));
            # Search for the next available test.
            redo;
          }

          # Limit number of parallell NDB tests
          if ($t->{ndb_test} and $num_ndb_tests >= $max_ndb) {
            next;
          }

          # Second best choice is the first that does not fulfill
          # any of the above conditions
          if (!defined $second_best) {
            $second_best = $i;
          }

          # Smart allocation of next test within this thread.
          if ($opt_reorder and $opt_parallel > 1 and defined $result) {
            my $wid = $result->{worker};
            # Reserved for other thread, try next
            next if (defined $t->{reserved} and $t->{reserved} != $wid);

            if (!defined $t->{reserved}) {
              # Force-restart not relevant when comparing *next* test
              $t->{criteria} =~ s/force-restart$/no-restart/;
              my $criteria = $t->{criteria};
              # Reserve similar tests for this worker, but not too many
              my $maxres = (@$tests - $i) / $opt_parallel + 1;

              for (my $j = $i + 1 ; $j <= $i + $maxres ; $j++) {
                my $tt = $tests->[$j];
                last unless defined $tt;
                last if $tt->{criteria} ne $criteria;
                $tt->{reserved} = $wid;
              }
            }
          }

          # At this point we have found next suitable test
          $next = splice(@$tests, $i, 1);
          last;
        }

        # Use second best choice if no other test has been found
        if (!$next and defined $second_best) {
          mtr_error("Internal error, second best too large($second_best)")
            if $second_best > $#$tests;
          $next = splice(@$tests, $second_best, 1);
          delete $next->{reserved};
        }

        if ($next) {
          # We don't need this any more
          delete $next->{criteria};
          $next->write_test($sock, 'TESTCASE');
          $running{ $next->key() } = $next;
          $num_ndb_tests++ if ($next->{ndb_test});
        } else {
          # Keep track of the number of child processes completed. Last
          # one will be used to run the non-parallel tests at the end.
          if ($completed_wid_count < $opt_parallel) {
            $completed_wid_count++;
          }

          # Check if there exist any non-parallel tests which should
          # be run using the last active worker process.
          if (int(@$non_parallel_tests) > 0 and
              $completed_wid_count == $opt_parallel) {
            # Fetch the next test to run from non_parallel_tests list
            $next = shift @$non_parallel_tests;

            # We don't need this any more
            delete $next->{criteria};

            $next->write_test($sock, 'TESTCASE');
            $running{ $next->key() } = $next;
            $num_ndb_tests++ if ($next->{ndb_test});
          } else {
            # No more test, tell child to exit
            print $sock "BYE\n";
          }
        }
      }
    }

    # Check if test suite timer expired
    if (has_expired($suite_timeout)) {
      mtr_report_stats("Timeout", $completed, 1);
      mtr_report("Test suite timeout! Terminating...");
      return undef;
    }
  }
}

# This is the main loop for the worker thread (which, as mentioned, is
# actually a separate process except on Windows).
#
# Its main loop reads messages from the main thread, which are either
# 'TESTCASE' with details on a test to run (also read with
# My::Test::read_test()) or 'BYE' which will make the worker clean up
# and send a 'SPENT' message. If running with valgrind, it also looks
# for valgrind reports and sends 'VALGREP' if any were found.
sub run_worker ($) {
  my ($server_port, $thread_num) = @_;

  $SIG{INT} = sub { exit(1); };

  # Connect to server
  my $server = new IO::Socket::INET(PeerAddr => 'localhost',
                                    PeerPort => $server_port,
                                    Proto    => 'tcp');
  mtr_error("Could not connect to server at port $server_port: $!")
    unless $server;

  # Set worker name
  report_option('name', "worker[$thread_num]");

  # Set different ports per thread
  set_build_thread_ports($thread_num);

  # Turn off verbosity in workers, unless explicitly specified
  report_option('verbose', undef) if ($opt_verbose == 0);

  environment_setup();

  # Read hello from server which it will send when shared
  # resources have been setup
  my $hello = <$server>;

  setup_vardir();
  check_running_as_root();

  if (using_extern()) {
    create_config_file_for_extern(%opts_extern);
  }

  # Ask server for first test
  print $server "START\n";

  mark_time_used('init');

  while (my $line = <$server>) {
    chomp($line);
    if ($line eq 'TESTCASE') {
      my $test = My::Test::read_test($server);

      # Clear comment and logfile, to avoid reusing them from previous test
      delete($test->{'comment'});
      delete($test->{'logfile'});

      # A sanity check. Should this happen often we need to look at it.
      if (defined $test->{reserved} && $test->{reserved} != $thread_num) {
        my $tres = $test->{reserved};
        mtr_warning("Test reserved for w$tres picked up by w$thread_num");
      }
      $test->{worker} = $thread_num if $opt_parallel > 1;

      run_testcase($test);

      # Send it back, now with results set
      $test->write_test($server, 'TESTRESULT');
      mark_time_used('restart');
    } elsif ($line eq 'BYE') {
      mtr_report("Server said BYE");
      stop_all_servers($opt_shutdown_timeout);
      mark_time_used('restart');

      my $valgrind_reports = 0;
      if ($opt_valgrind_mysqld or $opt_sanitize) {
        $valgrind_reports = valgrind_exit_reports();
        print $server "VALGREP\n" if $valgrind_reports;
      }

      if ($opt_gprof) {
        gprof_collect(find_mysqld($basedir), keys %gprof_dirs);
      }

      mark_time_used('admin');
      print_times_used($server, $thread_num);
      exit($valgrind_reports);
    } else {
      mtr_error("Could not understand server, '$line'");
    }
  }

  stop_all_servers();
  exit(1);
}

# Create a directory to store build thread id files
sub create_unique_id_dir() {
  if (IS_WINDOWS) {
    # Try to use machine-wide directory location for unique IDs,
    # $ALLUSERSPROFILE . IF it is not available, fallback to $TEMP
    # which is typically a per-user temporary directory
    if (exists $ENV{'ALLUSERSPROFILE'} && -w $ENV{'ALLUSERSPROFILE'}) {
      $build_thread_id_dir = $ENV{'ALLUSERSPROFILE'} . "/mysql-unique-ids";
    } else {
      $build_thread_id_dir = $ENV{'TEMP'} . "/mysql-unique-ids";
    }
  } else {
    $build_thread_id_dir = "/tmp/mysql-unique-ids";
  }

  # Check if directory already exists
  if (!-d $build_thread_id_dir) {
    # If there is a file with the reserved directory name, just
    # delete the file.
    if (-e $build_thread_id_dir) {
      unlink($build_thread_id_dir);
    }

    mkdir $build_thread_id_dir;
    chmod 0777, $build_thread_id_dir;

    die "Can't create directory $build_thread_id_dir: $!"
      if (!-d $build_thread_id_dir);
  }
}

# Remove all the unique files created to reserve ports.
sub clean_unique_id_dir () {
  open(FH, "<", $build_thread_id_file) or
    die "Can't open file $build_thread_id_file: $!";

  while (<FH>) {
    chomp($_);
    next if ($_ =~ /# Unique id file paths/);
    unlink $_ or warn "Cannot unlink file $_ : $!";
  }

  close(FH);
  unlink($build_thread_id_file) or
    die "Can't delete file $build_thread_id_file: $!";
}

# Remove redundant entries from build thread id file.
sub remove_redundant_thread_id_file_locations() {
  my $build_thread_id_tmp_file =
    "$build_thread_id_dir/" . $$ . "_unique_ids_tmp.log";

  open(RH, "<", $build_thread_id_file);
  open(WH, ">", $build_thread_id_tmp_file);

  my %file_location;
  while (<RH>) {
    print WH if not $file_location{$_}++;
  }

  close(RH);
  close(WH);

  File::Copy::move($build_thread_id_tmp_file, $build_thread_id_file);
}

sub ignore_option {
  my ($opt, $value) = @_;
  mtr_report("Ignoring option '$opt'");
}

# Setup any paths that are $opt_vardir related
sub set_vardir {
  my ($vardir) = @_;

  $opt_vardir        = $vardir;
  $path_vardir_trace = $opt_vardir;

  # Chop off any "c:", DBUG likes a unix path ex: c:/src/... => /src/...
  $path_vardir_trace =~ s/^\w://;

  # Location of my.cnf that all clients use
  $path_config_file = "$opt_vardir/my.cnf";

  $path_testlog         = "$opt_vardir/log/mysqltest.log";
  $path_current_testlog = "$opt_vardir/log/current_test";
}

sub print_global_resfile {
  resfile_global("bootstrap",        \@opt_extra_bootstrap_opt);
  resfile_global("callgrind",        $opt_callgrind ? 1 : 0);
  resfile_global("check-testcases",  $opt_check_testcases ? 1 : 0);
  resfile_global("compress",         $opt_compress ? 1 : 0);
  resfile_global("cursor-protocol",  $opt_cursor_protocol ? 1 : 0);
  resfile_global("debug",            $opt_debug ? 1 : 0);
  resfile_global("fast",             $opt_fast ? 1 : 0);
  resfile_global("force-restart",    $opt_force_restart ? 1 : 0);
  resfile_global("gcov",             $opt_gcov ? 1 : 0);
  resfile_global("gprof",            $opt_gprof ? 1 : 0);
  resfile_global("helgrind",         $opt_helgrind ? 1 : 0);
  resfile_global("max-connections",  $opt_max_connections);
  resfile_global("mem",              $opt_mem ? 1 : 0);
  resfile_global("mysqld",           \@opt_extra_mysqld_opt);
  resfile_global("mysqltest",        \@opt_extra_mysqltest_opt);
  resfile_global("no-skip",          $opt_no_skip ? 1 : 0);
  resfile_global("parallel",         $opt_parallel);
  resfile_global("product",          "MySQL");
  resfile_global("ps-protocol",      $opt_ps_protocol ? 1 : 0);
  resfile_global("reorder",          $opt_reorder ? 1 : 0);
  resfile_global("repeat",           $opt_repeat);
  resfile_global("sanitize",         $opt_sanitize ? 1 : 0);
  resfile_global("shutdown-timeout", $opt_shutdown_timeout ? 1 : 0);
  resfile_global("sleep",            $opt_sleep);
  resfile_global("sp-protocol",      $opt_sp_protocol ? 1 : 0);
  resfile_global("ssl",              $opt_ssl ? 1 : 0);
  resfile_global("start_time",       isotime $^T);
  resfile_global("suite-opt",        $opt_suite_opt);
  resfile_global("suite-timeout",    $opt_suite_timeout);
  resfile_global("summary-report",   $opt_summary_report);
  resfile_global("test-progress",    $opt_test_progress ? 1 : 0);
  resfile_global("testcase-timeout", $opt_testcase_timeout);
  resfile_global("tmpdir",           $opt_tmpdir);
  resfile_global("user",             $opt_user);
  resfile_global("user_id",          $<);
  resfile_global("valgrind",         $opt_valgrind ? 1 : 0);
  resfile_global("vardir",           $opt_vardir);
  resfile_global("view-protocol",    $opt_view_protocol ? 1 : 0);
  resfile_global("warnings",         $opt_warnings ? 1 : 0);
  resfile_global("xml-report",       $opt_xml_report);

  # Somewhat hacky code to convert numeric version back to dot notation
  my $v1 = int($mysql_version_id / 10000);
  my $v2 = int(($mysql_version_id % 10000) / 100);
  my $v3 = $mysql_version_id % 100;
  resfile_global("version", "$v1.$v2.$v3");
}

# Parses the command line arguments.
#
# Any new options added must be listed in the %options hash table.
# After parsing, there's a long list of sanity checks, handling of
# option inter-dependencies and setting of global variables like
# $basedir and $bindir. It also parses "experimental" files and
# performs various other setup tasks.
sub command_line_setup {
  my $opt_comment;
  my $opt_usage;
  my $opt_list_options;

  # Read the command line options
  # Note: Keep list in sync with usage at end of this file
  Getopt::Long::Configure("pass_through");
  my %options = (
    # Control what engine/variation to run
    'compress'              => \$opt_compress,
    'cursor-protocol'       => \$opt_cursor_protocol,
    'explain-protocol'      => \$opt_explain_protocol,
    'json-explain-protocol' => \$opt_json_explain_protocol,
    'opt-trace-protocol'    => \$opt_trace_protocol,
    'ps-protocol'           => \$opt_ps_protocol,
    'skip-ssl'              => \$opt_skip_ssl,
    'sp-protocol'           => \$opt_sp_protocol,
    'ssl|with-openssl'      => \$opt_ssl,
    'view-protocol'         => \$opt_view_protocol,
    'vs-config=s'           => \$opt_vs_config,

    # Max number of parallel threads to use
    'parallel=s' => \$opt_parallel,

    # Option to run the tests sourcing 'not_parallel.inc' file
    'non-parallel-test' => \$opt_non_parallel_test,

    # Config file to use as template for all tests
    'defaults-file=s' => \&collect_option,

    # Extra config file to append to all generated configs
    'defaults-extra-file=s' => \&collect_option,

    # Control what test suites or cases to run
    'big-test'                 => \$opt_big_test,
    'combination=s'            => \@opt_combinations,
    'do-suite=s'               => \$opt_do_suite,
    'do-test=s'                => \&collect_option,
    'experimental=s'           => \@opt_experimentals,
    'force'                    => \$opt_force,
    'ndb|include-ndbcluster'   => \$opt_include_ndbcluster,
    'no-skip'                  => \$opt_no_skip,
    'only-big-test'            => \$opt_only_big_test,
    'skip-combinations'        => \&collect_option,
    'skip-im'                  => \&ignore_option,
    'skip-ndbcluster|skip-ndb' => \$opt_skip_ndbcluster,
    'skip-rpl'                 => \&collect_option,
    'skip-sys-schema'          => \$opt_skip_sys_schema,
    'skip-test=s'              => \&collect_option,
    'start-from=s'             => \&collect_option,
    'suite|suites=s'           => \$opt_suites,
    'with-ndbcluster-only'     => \&collect_option,

    # Specify ports
    'build-thread|mtr-build-thread=i' => \$opt_build_thread,
    'mysqlx-port=i'                   => \$opt_mysqlx_baseport,
    'port-base|mtr-port-base=i'       => \$opt_port_base,

    # Test case authoring
    'check-testcases!' => \$opt_check_testcases,
    'mark-progress'    => \$opt_mark_progress,
    'record'           => \$opt_record,
    'test-progress'    => \$opt_test_progress,

    # Extra options used when starting mysqld
    'mysqld=s'     => \@opt_extra_mysqld_opt,
    'mysqld-env=s' => \@opt_mysqld_envs,

    # Extra options used when bootstrapping mysqld
    'bootstrap=s' => \@opt_extra_bootstrap_opt,

    # Run test on running server
    'extern=s' => \%opts_extern,    # Append to hash

    # Extra options used when running test clients
    'mysqltest=s' => \@opt_extra_mysqltest_opt,

    # Debugging
    'boot-dbx'           => \$opt_boot_dbx,
    'boot-ddd'           => \$opt_boot_ddd,
    'boot-gdb'           => \$opt_boot_gdb,
    'client-dbx'         => \$opt_client_dbx,
    'client-ddd'         => \$opt_client_ddd,
    'client-debugger=s'  => \$opt_client_debugger,
    'client-gdb'         => \$opt_client_gdb,
    'client-lldb'        => \$opt_client_lldb,
    'dbx'                => \$opt_dbx,
    'ddd'                => \$opt_ddd,
    'debug'              => \$opt_debug,
    'debug-common'       => \$opt_debug_common,
    'debug-server'       => \$opt_debug_server,
    'debugger=s'         => \$opt_debugger,
    'gdb'                => \$opt_gdb,
    'lldb'               => \$opt_lldb,
    'manual-boot-gdb'    => \$opt_manual_boot_gdb,
    'manual-dbx'         => \$opt_manual_dbx,
    'manual-ddd'         => \$opt_manual_ddd,
    'manual-debug'       => \$opt_manual_debug,
    'manual-gdb'         => \$opt_manual_gdb,
    'manual-lldb'        => \$opt_manual_lldb,
    'max-save-core=i'    => \$opt_max_save_core,
    'max-save-datadir=i' => \$opt_max_save_datadir,
    'max-test-fail=i'    => \$opt_max_test_fail,
    'strace-client'      => \$opt_strace_client,
    'strace-server'      => \$opt_strace_server,

    # Coverage, profiling etc
    'callgrind'             => \$opt_callgrind,
    'debug-sync-timeout=i'  => \$opt_debug_sync_timeout,
    'gcov'                  => \$opt_gcov,
    'gprof'                 => \$opt_gprof,
    'helgrind'              => \$opt_helgrind,
    'sanitize'              => \$opt_sanitize,
    'valgrind-clients'      => \$opt_valgrind_clients,
    'valgrind-mysqld'       => \$opt_valgrind_mysqld,
    'valgrind-mysqltest'    => \$opt_valgrind_mysqltest,
    'valgrind-option=s'     => \@valgrind_args,
    'valgrind-path=s'       => \$opt_valgrind_path,
    'valgrind|valgrind-all' => \$opt_valgrind,
    'valgrind-options=s'    => sub {
      my ($opt, $value) = @_;
      # Deprecated option unless it's what we know pushbuild uses
      if (option_equals($value, "--gen-suppressions=all --show-reachable=yes"))
      {
        push(@valgrind_args, $_) for (split(' ', $value));
        return;
      }
      die("--valgrind-options=s is deprecated. Use ",
          "--valgrind-option=s, to be specified several",
          " times if necessary");
    },

    # Directories
    'clean-vardir'    => \$opt_clean_vardir,
    'client-bindir=s' => \$path_client_bindir,
    'client-libdir=s' => \$path_client_libdir,
    'mem'             => \$opt_mem,
    'tmpdir=s'        => \$opt_tmpdir,
    'vardir=s'        => \$opt_vardir,

    # Misc
    'charset-for-testdb=s'  => \$opt_charset_for_testdb,
    'colored-diff'          => \$opt_colored_diff,
    'comment=s'             => \$opt_comment,
    'default-myisam!'       => \&collect_option,
    'disk-usage!'           => \&report_option,
    'enable-disabled'       => \&collect_option,
    'fast'                  => \$opt_fast,
    'force-restart'         => \$opt_force_restart,
    'help|h'                => \$opt_usage,
    'max-connections=i'     => \$opt_max_connections,
    'print-testcases'       => \&collect_option,
    'reorder!'              => \$opt_reorder,
    'repeat=i'              => \$opt_repeat,
    'report-features'       => \$opt_report_features,
    'report-times'          => \$opt_report_times,
    'report-unstable-tests' => \$opt_report_unstable_tests,
    'result-file'           => \$opt_resfile,
    'retry-failure=i'       => \$opt_retry_failure,
    'retry=i'               => \$opt_retry,
    'shutdown-timeout=i'    => \$opt_shutdown_timeout,
    'sleep=i'               => \$opt_sleep,
    'start'                 => \$opt_start,
    'start-and-exit'        => \$opt_start_exit,
    'start-dirty'           => \$opt_start_dirty,
    'stress=s'              => \$opt_stress,
    'suite-opt=s'           => \$opt_suite_opt,
    'suite-timeout=i'       => \$opt_suite_timeout,
    'testcase-timeout=i'    => \$opt_testcase_timeout,
    'timediff'              => \&report_option,
    'timer!'                => \&report_option,
    'timestamp'             => \&report_option,
    'unit-tests!'           => \$opt_ctest,
    'unit-tests-report!'    => \$opt_ctest_report,
    'user-args'             => \$opt_user_args,
    'user=s'                => \$opt_user,
    'verbose'               => \$opt_verbose,
    'verbose-restart'       => \&report_option,
    'wait-all'              => \$opt_wait_all,
    'warnings!'             => \$opt_warnings,

    # list-options is internal, not listed in help
    'do-test-list=s'   => \$opt_do_test_list,
    'list-options'     => \$opt_list_options,
    'skip-test-list=s' => \$opt_skip_test_list,
    'summary-report=s' => \$opt_summary_report,
    'xml-report=s'     => \$opt_xml_report);

  GetOptions(%options) or usage("Can't read options");

  usage("") if $opt_usage;
  list_options(\%options) if $opt_list_options;

  # Setup verbosity if verbose option is enabled.
  if ($opt_verbose) {
    report_option('verbose', $opt_verbose);
  }

  if (-d "../sql") {
    $source_dist = 1;
  }

  # Find the absolute path to the test directory
  $glob_mysql_test_dir = cwd();
  if ($glob_mysql_test_dir =~ / /) {
    die("Working directory \"$glob_mysql_test_dir\" contains space\n" .
        "Bailing out, cannot function properly with space in path");
  }

  if (IS_CYGWIN) {
    # Use mixed path format i.e c:/path/to/
    $glob_mysql_test_dir = mixed_path($glob_mysql_test_dir);
  }

  # In most cases, the base directory we find everything relative to,
  # is the parent directory of the "mysql-test" directory. For source
  # distributions, TAR binary distributions and some other packages.
  $basedir = dirname($glob_mysql_test_dir);

  # In the RPM case, binaries and libraries are installed in the
  # default system locations, instead of having our own private base
  # directory. And we install "/usr/share/mysql-test". Moving up one
  # more directory relative to "mysql-test" gives us a usable base
  # directory for RPM installs.
  if (!$source_dist and !-d "$basedir/bin") {
    $basedir = dirname($basedir);
  }

  # Respect MTR_BINDIR variable, which is typically set in to the
  # build directory in out-of-source builds.
  $bindir = $ENV{MTR_BINDIR} || $basedir;

  # Look for the client binaries directory
  if ($path_client_bindir) {
    # --client-bindir=path set on command line, check that the path exists
    $path_client_bindir = mtr_path_exists($path_client_bindir);
  } else {
    $path_client_bindir =
      mtr_path_exists(vs_config_dirs('runtime_output_directory', ''),
                      "$bindir/bin");
  }

  if (using_extern()) {
    # Connect to the running mysqld and find out what it supports
    collect_mysqld_features_from_running_server();
  } else {
    # Run the mysqld to find out what features are available
    collect_mysqld_features();
  }

  # Look for language files and charsetsdir, use same share
  $path_language = mtr_path_exists("$bindir/share/mysql", "$bindir/share");
  my $path_share = $path_language;

  @share_locations =
    ("share/mysql", "share/mysql-" . $mysql_base_version, "share");

  $path_charsetsdir = my_find_dir($basedir, \@share_locations, "charsets");

  ($auth_plugin) = find_plugin("auth_test_plugin", "plugin_output_directory");

  # On windows, backslashes in the file name argument to "load data
  # infile" statement should be specified either as forward slashes or
  # doubled backslashes. If vardir path contains backslashes,
  # "check-warnings.test" will fail with parallel > 1, because the
  # path to error log file is calculated using vardir path and this
  # path is used with "load data infile" statement.
  # Replace '\' with '/' on windows.
  $opt_vardir =~ s/\\/\//g if (defined $opt_vardir and IS_WINDOWS);

  # --debug[-common] implies we run debug server
  $opt_debug_server = 1 if $opt_debug || $opt_debug_common;

  if ($opt_comment) {
    mtr_report();
    mtr_print_thick_line('#');
    mtr_report("# $opt_comment");
    mtr_print_thick_line('#');
  }

  if (@opt_experimentals) {
    # $^O on Windows considered not generic enough
    my $plat = (IS_WINDOWS) ? 'windows' : $^O;

    # Read the list of experimental test cases from the files specified
    # on the command line.
    $experimental_test_cases = [];
    foreach my $exp_file (@opt_experimentals) {
      open(FILE, "<", $exp_file) or
        mtr_error("Can't read experimental file: $exp_file");
      mtr_report("Using experimental file: $exp_file");

      while (<FILE>) {
        chomp;
        # Remove comments (# foo) at the beginning of the line, or after a
        # blank at the end of the line.
        s/(\s+|^)#.*$//;

        # If @ platform specifier given, use this entry only if it contains
        # @<platform> or @!<xxx> where xxx != platform.
        if (/\@.*/) {
          next if (/\@!$plat/);
          next unless (/\@$plat/ or /\@!/);
          # Then remove @ and everything after it
          s/\@.*$//;
        }

        # Remove whitespace
        s/^\s+//;
        s/\s+$//;

        # If nothing left, don't need to remember this line
        if ($_ eq "") {
          next;
        }

        # Remember what is left as the name of another test case that
        # should be treated as experimental.
        print " - $_\n";
        push @$experimental_test_cases, $_;
      }
      close FILE;
    }
  }

  foreach my $arg (@ARGV) {
    if ($arg =~ /^--skip-/) {
      push(@opt_extra_mysqld_opt, $arg);
    } elsif ($arg =~ /^--$/) {
      # It is an effect of setting 'pass_through' in option processing
      # that the lone '--' separating options from arguments survives,
      # simply ignore it.
    } elsif ($arg =~ /^-/) {
      usage("Invalid option \"$arg\"");
    } else {
      push(@opt_cases, $arg);
    }
  }

  # Disable syslog / EventLog in normal (non-bootstrap) operation.
  push(@opt_extra_mysqld_opt, "--log-syslog=0");

  # Find out type of logging that are being used
  foreach my $arg (@opt_extra_mysqld_opt) {
    if ($arg =~ /binlog[-_]format=(\S+)/) {
      # Save this for collect phase
      collect_option('binlog-format', $1);
      mtr_report("Using binlog format '$1'");
    }
  }

  # Read the file and store it in a string.
  if ($opt_no_skip) {
    $excluded_string = '';
    my @noskip_exclude_lists = ('include/excludenoskip.list');
    my $i_noskip_exclude_list =
      '../internal/mysql-test/include/i_excludenoskip.list';
    push(@noskip_exclude_lists, $i_noskip_exclude_list)
      if (-e $i_noskip_exclude_list);

    foreach my $excludedList (@noskip_exclude_lists) {
      open(my $fh, '<', $excludedList) or
        die "no-skip option cannot run without '$excludedList' $!";
      while (<$fh>) {
        chomp $_;
        $excluded_string .= $_ . "," unless ($_ =~ /^\s*$/ or $_ =~ /^#/);
      }
      close $fh;
    }
    chop $excluded_string;
  }

  # Find out default storage engine being used(if any)
  foreach my $arg (@opt_extra_mysqld_opt) {
    if ($arg =~ /default[-_]storage[-_]engine=(\S+)/) {
      # Save this for collect phase
      collect_option('default-storage-engine', $1);
      mtr_report("Using default engine '$1'");
    }
    if ($arg =~ /default[-_]tmp-storage[-_]engine=(\S+)/) {
      # Save this for collect phase
      collect_option('default-tmp-storage-engine', $1);
      mtr_report("Using default tmp engine '$1'");
    }
  }

  if ($opt_port_base ne "auto") {
    if (my $rem = $opt_port_base % 10) {
      mtr_warning("Port base $opt_port_base rounded down to multiple of 10");
      $opt_port_base -= $rem;
    }
    $opt_build_thread = $opt_port_base / 10 - 1000;
  }

  # Check if we should speed up tests by trying to run on tmpfs
  if ($opt_mem) {
    mtr_error("Can't use --mem and --vardir at the same time ")
      if $opt_vardir;

    mtr_error("Can't use --mem and --tmpdir at the same time ")
      if $opt_tmpdir;

    # Disable '--mem' option on Windows
    if (IS_WINDOWS) {
      mtr_report("Turning off '--mem' option since it is not supported " .
                 "on Windows.");
      $opt_mem = undef;
    }
    # Disable '--mem' option on MacOS
    elsif (IS_MAC) {
      mtr_report(
         "Turning off '--mem' option since it is not supported " . "on MacOS.");
      $opt_mem = undef;
    } else {
      # Search through the list of locations that are known
      # to be "fast disks" to find a suitable location.
      my @tmpfs_locations = ("/dev/shm", "/run/shm", "/tmp");

      # Value set for env variable MTR_MEM=[DIR] is looked as first location.
      unshift(@tmpfs_locations, $ENV{'MTR_MEM'}) if defined $ENV{'MTR_MEM'};

      foreach my $fs (@tmpfs_locations) {
        if (-d $fs and !-l $fs) {
          my $template = "var_${opt_build_thread}_XXXX";
          $opt_mem = tempdir($template, DIR => $fs, CLEANUP => 0);
          last;
        }
      }

      # Check if opt_mem is set to any of the built-in list of tmpfs
      # locations (/dev/shm, /run/shm, /tmp).
      if ($opt_mem eq 1) {
        mtr_report("Couldn't find any of the built-in list of tmpfs " .
                   "locations(/dev/shm, /run/shm, /tmp), turning off " .
                   "'--mem' option.");
        $opt_mem = undef;
      }
    }
  }

  # Set the "var/" directory, the base for everything else
  if (defined $ENV{MTR_BINDIR}) {
    $default_vardir = "$ENV{MTR_BINDIR}/mysql-test/var";
  } else {
    $default_vardir = "$glob_mysql_test_dir/var";
  }

  if (!$opt_vardir) {
    $opt_vardir = $default_vardir;
  }

  # We make the path absolute, as the server will do a chdir() before usage
  unless ($opt_vardir =~ m,^/, or
          (IS_WINDOWS and $opt_vardir =~ m,^[a-z]:[/\\],i)) {
    # Make absolute path, relative test dir
    $opt_vardir = "$glob_mysql_test_dir/$opt_vardir";
  }

  set_vardir($opt_vardir);

  # Check if "parallel" options is set
  if (not defined $opt_parallel) {
    if ($opt_start or $opt_start_dirty) {
      # Set parallel value to 1
      $opt_parallel = 1;
    } else {
      # Set parallel value to "auto"
      $opt_parallel = "auto";
    }
  } else {
    my $flag = 0;
    # Check if parallel value is a positive number or "auto".
    if ($opt_parallel =~ /^[0-9]+$/) {
      # Numeric value, can't be less than '1'
      $flag = 1 if ($opt_parallel < 1);
    } else {
      # String value and should be "auto"
      $flag = 1 if ($opt_parallel ne "auto");
    }

    mtr_error("Invalid value '$opt_parallel' for '--parallel' option, " .
              "use 'auto' or a positive number.")
      if $flag;
  }

  # Set the "tmp" directory
  if (!$opt_tmpdir) {
    $opt_tmpdir = "$opt_vardir/tmp" unless $opt_tmpdir;

    my $res = check_socket_path_length("$opt_tmpdir/mysql_testsocket.sock",
                                       $opt_parallel);

    if ($res) {
      mtr_report("Too long tmpdir path '$opt_tmpdir'",
                 " creating a shorter one...");

      # Create temporary directory in standard location for temporary files
      $opt_tmpdir = tempdir(TMPDIR => 1, CLEANUP => 0);
      mtr_report(" - Using tmpdir: '$opt_tmpdir'\n");

      # Remember pid that created dir so it's removed by correct process
      $opt_tmpdir_pid = $$;
    }
  }

  # Remove ending slash if any
  $opt_tmpdir =~ s,/+$,,;

  # fast option
  if ($opt_fast) {
    # Kill processes instead of nice shutdown
    $opt_shutdown_timeout = 0;
  }

  # Big test flags
  if ($opt_only_big_test and $opt_big_test) {
    # Disabling only-big-test option if both big-test and
    # only-big-test options are passed.
    mtr_report("Turning off --only-big-test");
    $opt_only_big_test = 0;
  }

  # Enable --big-test and option when test cases are specified command line.
  if (@opt_cases) {
    $opt_big_test = 1 if !$opt_big_test;
  }

  $ENV{'BIG_TEST'} = 1 if ($opt_big_test or $opt_only_big_test);

  # Gcov flag
  if (($opt_gcov or $opt_gprof) and !$source_dist) {
    mtr_error("Coverage test needs the source - please use source dist");
  }

  # Check debug related options
  if ($opt_gdb ||
      $opt_client_gdb      ||
      $opt_ddd             ||
      $opt_client_ddd      ||
      $opt_manual_gdb      ||
      $opt_manual_lldb     ||
      $opt_manual_ddd      ||
      $opt_manual_debug    ||
      $opt_dbx             ||
      $opt_client_dbx      ||
      $opt_manual_dbx      ||
      $opt_debugger        ||
      $opt_client_debugger ||
      $opt_manual_boot_gdb) {
    # Indicate that we are using debugger
    $glob_debugger = 1;

    if (using_extern()) {
      mtr_error("Can't use --extern when using debugger");
    }

    # Set one week timeout (check-testcase timeout will be 1/10th)
    $opt_testcase_timeout = 7 * 24 * 60;
    $opt_suite_timeout    = 7 * 24 * 60;
    $opt_shutdown_timeout = 24 * 60;         # One day to shutdown
    $opt_shutdown_timeout = 24 * 60;
    $opt_start_timeout    = 24 * 60 * 60;    # One day for PID file creation
  }

  # Modified behavior with --start options
  if ($opt_start or $opt_start_dirty or $opt_start_exit) {
    collect_option('quick-collect', 1);
    $start_only = 1;
  }

  # Check use of user-args
  if ($opt_user_args) {
    mtr_error("--user-args only valid with --start options")
      unless $start_only;
    mtr_error("--user-args cannot be combined with named suites or tests")
      if $opt_suites || @opt_cases;
  }

  # Set default values for opt_ctest (--unit-tests)
  if ($opt_ctest == -1) {
    if (defined $opt_ctest_report && $opt_ctest_report) {
      # Turn on --unit-tests by default if --unit-tests-report is used
      $opt_ctest = 1;
    } elsif ($opt_suites || @opt_cases) {
      # Don't run ctest if tests or suites named
      $opt_ctest = 0;
    } elsif (defined $ENV{PB2WORKDIR}) {
      # Override: disable if running in the PB test environment
      $opt_ctest = 0;
    }
  }

  # Check use of wait-all
  if ($opt_wait_all && !$start_only) {
    mtr_error("--wait-all can only be used with --start options");
  }

  # Gather stress-test options and modify behavior
  if ($opt_stress) {
    $opt_stress =~ s/,/ /g;
    $opt_user_args = 1;
    mtr_error("--stress cannot be combined with named ordinary suites or tests")
      if $opt_suites || @opt_cases;

    $opt_suites       = "stress";
    @opt_cases        = ("wrapper");
    $ENV{MST_OPTIONS} = $opt_stress;
    $opt_ctest        = 0;
  }

  # Check timeout arguments
  mtr_error("Invalid value '$opt_testcase_timeout' supplied " .
            "for option --testcase-timeout")
    if ($opt_testcase_timeout <= 0);
  mtr_error("Invalid value '$opt_suite_timeout' supplied " .
            "for option --testsuite-timeout")
    if ($opt_suite_timeout <= 0);

  # Check trace protocol option
  if ($opt_trace_protocol) {
    push(@opt_extra_mysqld_opt, "--optimizer_trace=enabled=on,one_line=off");
    # Some queries yield big traces:
    push(@opt_extra_mysqld_opt, "--optimizer-trace-max-mem-size=1000000");
  }

  # Check valgrind arguments
  if ($opt_valgrind or $opt_valgrind_path or @valgrind_args) {
    mtr_report("Turning on valgrind for all executables");
    $opt_valgrind        = 1;
    $opt_valgrind_mysqld = 1;
    # Enable this when mysqlpump and mysqlbinlog are fixed.
    # $opt_valgrind_clients = 1;
    $opt_valgrind_mysqltest = 1;

    # Increase the timeouts when running with valgrind
    $opt_testcase_timeout   *= 10;
    $opt_suite_timeout      *= 6;
    $opt_start_timeout      *= 10;
    $opt_debug_sync_timeout *= 10;
  } elsif ($opt_valgrind_mysqld) {
    mtr_report("Turning on valgrind for mysqld(s) only");
    $opt_valgrind = 1;
  } elsif ($opt_valgrind_clients) {
    mtr_report("Turning on valgrind for test clients");
    $opt_valgrind = 1;
  } elsif ($opt_valgrind_mysqltest) {
    mtr_report("Turning on valgrind for mysqltest and mysql_client_test only");
    $opt_valgrind = 1;
  }

  if ($opt_callgrind) {
    mtr_report("Turning on valgrind with callgrind for mysqld(s)");
    $opt_valgrind        = 1;
    $opt_valgrind_mysqld = 1;

    push(@valgrind_args, "--tool=callgrind", "--trace-children=yes");

    # Increase the timeouts when running with callgrind
    $opt_testcase_timeout   *= 10;
    $opt_suite_timeout      *= 6;
    $opt_start_timeout      *= 10;
    $opt_debug_sync_timeout *= 10;
  }

  if ($opt_helgrind) {
    mtr_report("Turning on valgrind with helgrind for mysqld(s)");
    $opt_valgrind        = 1;
    $opt_valgrind_mysqld = 1;

    push(@valgrind_args, "--tool=helgrind");

    # Checking for warnings takes too long time currently.
    mtr_report("Turning off --warnings to save time when helgrinding");
    $opt_warnings = 0;
  }

  if ($opt_valgrind) {
    # Default to --tool=memcheck if no other tool has been explicitly
    # specified. From >= 2.1.2, this option is needed
    if (!@valgrind_args or !grep(/^--tool=/, @valgrind_args)) {
      # Set default valgrind options for memcheck, can be overriden by user
      unshift(@valgrind_args,
              ("--tool=memcheck", "--num-callers=16", "--show-reachable=yes"));
    }

    # Add suppression file if not specified
    if (!grep(/^--suppressions=/, @valgrind_args)) {
      push(@valgrind_args,
           "--suppressions=${glob_mysql_test_dir}/valgrind.supp")
        if -f "$glob_mysql_test_dir/valgrind.supp";
    }

    # Don't add --quiet; you will loose the summary reports.
    mtr_report("Running valgrind with options \"",
               join(" ", @valgrind_args), "\"");

    # Turn off check testcases to save time
    mtr_report("Turning off --check-testcases to save time when valgrinding");
    $opt_check_testcases = 0;
  }

  if ($opt_debug_common) {
    $opt_debug = 1;
    $debug_d   = "d,query,info,error,enter,exit";
  }

  if ($opt_strace_server && ($^O ne "linux")) {
    $opt_strace_server = 0;
    mtr_warning("Strace only supported in Linux ");
  }

  if ($opt_strace_client && ($^O ne "linux")) {
    $opt_strace_client = 0;
    mtr_warning("Strace only supported in Linux ");
  }

  mtr_report("Checking supported features...");

  check_ndbcluster_support(\%mysqld_variables);
  check_ssl_support(\%mysqld_variables);
  check_debug_support(\%mysqld_variables);

  executable_setup();
}

# To make it easier for different devs to work on the same host, an
# environment variable can be used to control all ports. A small number
# is to be used, 0 - 16 or similar.
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
sub set_build_thread_ports($) {
  my $thread = shift || 0;

  # Number of unique build threads needed per MTR thread.
  my $build_threads_per_thread = int($ports_per_thread / 10);

  if (lc($opt_build_thread) eq 'auto') {
    # Start searching for build thread ids from here
    $build_thread = 300;
    my $max_parallel = $opt_parallel * $build_threads_per_thread;

    # Calucalte the upper limit value for build thread id
    my $build_thread_upper =
      $max_parallel > 39 ? $max_parallel + int($max_parallel / 2) : 49;

    # Check the number of available processors and accordingly set
    # the upper limit value for the build thread id.
    $build_thread_upper =
      $build_thread + ($ENV{NUMBER_OF_CPUS} > $build_thread_upper ?
                         $ENV{NUMBER_OF_CPUS} + int($ENV{NUMBER_OF_CPUS} / 2) :
                         $build_thread_upper);

    my $found_free = 0;
    while (!$found_free) {
      $build_thread = mtr_get_unique_id($build_thread, $build_thread_upper,
                                        $build_threads_per_thread);

      if (!defined $build_thread) {
        mtr_error("Could not get a unique build thread id");
      }

      for (my $i = 0 ; $i < $build_threads_per_thread ; $i++) {
        $found_free = check_ports_free($build_thread + $i);
        last if !$found_free;
      }

      # If not free, release and try from next number
      if (!$found_free) {
        mtr_release_unique_id();
        $build_thread++;
      }
    }
  } else {
    $build_thread =
      $opt_build_thread + ($thread - 1) * $build_threads_per_thread;
    for (my $i = 0 ; $i < $build_threads_per_thread ; $i++) {
      if (!check_ports_free($build_thread + $i)) {
        # Some port was not free(which one has already been printed)
        mtr_error("Some port(s) was not free");
      }
    }
  }

  $ENV{MTR_BUILD_THREAD} = $build_thread;

  # Calculate baseport
  $baseport = $build_thread * 10 + 10000;

  if (lc($opt_mysqlx_baseport) eq "auto") {
    if ($ports_per_thread > 10) {
      # Reserving last 10 ports in the current port range for X plugin.
      $mysqlx_baseport = $baseport + $ports_per_thread - 10;
    } else {
      # Reserving the last port in the range for X plugin
      $mysqlx_baseport = $baseport + 9;
    }
  } else {
    $mysqlx_baseport = $opt_mysqlx_baseport;
  }

  if ($baseport < 5001 or $baseport + $ports_per_thread - 1 >= 32767) {
    mtr_error("MTR_BUILD_THREAD number results in a port",
              "outside 5001 - 32767",
              "($baseport - $baseport + $ports_per_thread - 1)");
  }

  mtr_report("Using MTR_BUILD_THREAD $build_thread,",
       "with reserved ports $baseport.." . ($baseport + $ports_per_thread - 1));
}

# Runs a mysqld with options --verbose --help and extracts version
# number and variables from the output; variables are put into the
# $mysqld_variables hash to be used later, to determine if we have
# support for this or that feature.
sub collect_mysqld_features {
  my $found_variable_list_start = 0;
  my $use_tmpdir;
  if (defined $opt_tmpdir and -d $opt_tmpdir) {
    # Create the tempdir in $opt_tmpdir
    $use_tmpdir = $opt_tmpdir;
  }
  my $tmpdir = tempdir(CLEANUP => 0,
                       DIR     => $use_tmpdir);

  # Execute "mysqld --no-defaults --help --verbose" to get a list of
  # all features and settings. '--no-defaults' and '--skip-grant-tables'
  # are to avoid loading system-wide configs and plugins. '--datadir
  # must exist, mysqld will chdir into it.
  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--datadir=%s", mixed_path($tmpdir));
  mtr_add_arg($args, "--log-syslog=0");
  mtr_add_arg($args, "--secure-file-priv=\"\"");
  mtr_add_arg($args, "--skip-grant-tables");
  mtr_add_arg($args, "--help");
  mtr_add_arg($args, "--verbose");

  # Need --user=root if running as *nix root user
  if (!IS_WINDOWS and $> == 0) {
    mtr_add_arg($args, "--user=root");
  }

  my $exe_mysqld = find_mysqld($basedir);
  my $cmd        = join(" ", $exe_mysqld, @$args);
  my $list       = `$cmd`;

  foreach my $line (split('\n', $list)) {
    # First look for version
    if (!$mysql_version_id) {
      # Look for version
      my $exe_name = basename($exe_mysqld);
      mtr_verbose("exe_name: $exe_name");
      if ($line =~ /^\S*$exe_name\s\sVer\s([0-9]*)\.([0-9]*)\.([0-9]*)([^\s]*)/)
      {
        $mysql_version_id = $1 * 10000 + $2 * 100 + $3;
        # Some paths might be version specific
        $mysql_base_version =
          int($mysql_version_id / 10000) . "." .
          int(($mysql_version_id % 10000) / 100);
        mtr_report("MySQL Version $1.$2.$3");
        $mysql_version_extra = $4;
      }
    } else {
      if (!$found_variable_list_start) {
        # Look for start of variables list
        if ($line =~ /[\-]+\s[\-]+/) {
          $found_variable_list_start = 1;
        }
      } else {
        # Put variables into hash
        if ($line =~ /^([\S]+)[ \t]+(.*?)\r?$/) {
          # print "$1=\"$2\"\n";
          $mysqld_variables{$1} = $2;
        } else {
          # The variable list is ended with a blank line
          if ($line =~ /^[\s]*$/) {
            last;
          } else {
            # Send out a warning, we should fix the variables that has no
            # space between variable name and it's value or should it be
            # fixed width column parsing? It does not look like that in
            # function my_print_variables in my_getopt.c
            mtr_warning("Could not parse variable list line : $line");
          }
        }
      }
    }
  }

  rmtree($tmpdir);
  mtr_error("Could not find version of MySQL") unless $mysql_version_id;
  mtr_error("Could not find variabes list") unless $found_variable_list_start;

  # InnoDB is always enabled as of 5.7.
  $mysqld_variables{'innodb'} = "ON";
}

sub collect_mysqld_features_from_running_server () {
  my $mysql = mtr_exe_exists("$path_client_bindir/mysql");

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--user=%s", $opt_user);

  while (my ($option, $value) = each(%opts_extern)) {
    mtr_add_arg($args, "--$option=$value");
  }

  mtr_add_arg($args, "--silent");    # Tab separated output
  mtr_add_arg($args, "-e '%s'", "use mysql; SHOW GLOBAL VARIABLES");
  my $cmd = "$mysql " . join(' ', @$args);
  mtr_verbose("cmd: $cmd");

  my $list = `$cmd` or
    mtr_error("Could not connect to extern server using command: '$cmd'");
  foreach my $line (split('\n', $list)) {
    # Put variables into hash
    if ($line =~ /^([\S]+)[ \t]+(.*?)\r?$/) {
      # print "$1=\"$2\"\n";
      $mysqld_variables{$1} = $2;
    }
  }

  # InnoDB is always enabled as of 5.7.
  $mysqld_variables{'innodb'} = "ON";

  # Parse version
  my $version_str = $mysqld_variables{'version'};
  if ($version_str =~ /^([0-9]*)\.([0-9]*)\.([0-9]*)([^\s]*)/) {
    $mysql_version_id = $1 * 10000 + $2 * 100 + $3;
    # Some paths might be version specific
    $mysql_base_version =
      int($mysql_version_id / 10000) . "." .
      int(($mysql_version_id % 10000) / 100);
    mtr_report("MySQL Version $1.$2.$3");
    $mysql_version_extra = $4;
  }
  mtr_error("Could not find version of MySQL") unless $mysql_version_id;
}

sub find_mysqld {
  my ($mysqld_basedir) = $ENV{MTR_BINDIR} || @_;

  my @mysqld_names = ("mysqld");

  if ($opt_debug_server) {
    # Put mysqld-debug first in the list of binaries to look for
    mtr_verbose("Adding mysqld-debug first in list of binaries to look for");
    unshift(@mysqld_names, "mysqld-debug");
  }

  return
    my_find_bin($mysqld_basedir,
                [ "runtime_output_directory", "libexec", "sbin", "bin" ],
                [@mysqld_names]);
}

# Finds paths to various executables (other than mysqld) and sets
# the corresponding '$exe_xxx' variables.
sub executable_setup () {
  # Check if libtool is available in this distribution/clone
  # we need it when valgrinding or debugging non installed binary
  # Otherwise valgrind will valgrind the libtool wrapper or bash
  # and gdb will not find the real executable to debug
  if (-x "../libtool") {
    $exe_libtool = "../libtool";
    if ($opt_valgrind or $glob_debugger) {
      mtr_report("Using \"$exe_libtool\" when running valgrind or debugger");
    }
  }

  # Look for the client binaries
  $exe_mysqladmin = mtr_exe_exists("$path_client_bindir/mysqladmin");
  $exe_mysql      = mtr_exe_exists("$path_client_bindir/mysql");
  $exe_mysql_ssl_rsa_setup =
    mtr_exe_exists("$path_client_bindir/mysql_ssl_rsa_setup");

  if ($ndbcluster_enabled) {
    # Look for single threaded NDB
    $exe_ndbd =
      my_find_bin($bindir,
                  [ "runtime_output_directory", "libexec", "sbin", "bin" ],
                  "ndbd");

    # Look for multi threaded NDB
    $exe_ndbmtd =
      my_find_bin($bindir,
                  [ "runtime_output_directory", "libexec", "sbin", "bin" ],
                  "ndbmtd", NOT_REQUIRED);

    if ($exe_ndbmtd) {
      my $mtr_ndbmtd = $ENV{MTR_NDBMTD};
      if ($mtr_ndbmtd) {
        mtr_report(" - multi threaded ndbd found, will be used always");
        $exe_ndbd = $exe_ndbmtd;
      } else {
        mtr_report(
             " - multi threaded ndbd found, will be " . "used \"round robin\"");
      }
    }

    $exe_ndb_mgmd =
      my_find_bin($bindir,
                  [ "runtime_output_directory", "libexec", "sbin", "bin" ],
                  "ndb_mgmd");

    $exe_ndb_mgm =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ], "ndb_mgm");

    $exe_ndb_waiter =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ], "ndb_waiter");

  }

  if (defined $ENV{'MYSQL_TEST'}) {
    $exe_mysqltest = $ENV{'MYSQL_TEST'};
    print "===========================================================\n";
    print "WARNING:The mysqltest binary is fetched from $exe_mysqltest\n";
    print "===========================================================\n";
  } else {
    $exe_mysqltest = mtr_exe_exists("$path_client_bindir/mysqltest");
  }
}

sub client_debug_arg($$) {
  my ($args, $client_name) = @_;

  # Workaround for Bug #50627: drop any debug opt
  return if $client_name =~ /^mysqlbinlog/;

  if ($opt_debug) {
    mtr_add_arg($args, "--loose-debug=$debug_d:t:A,%s/log/%s.trace",
                $path_vardir_trace, $client_name);
  }
}

sub client_arguments ($;$) {
  my $client_name  = shift;
  my $group_suffix = shift;
  my $client_exe   = mtr_exe_exists("$path_client_bindir/$client_name");

  my $args;
  mtr_init_args(\$args);
  if ($opt_valgrind_clients) {
    valgrind_client_arguments($args, \$client_exe);
  }
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);

  if (defined($group_suffix)) {
    mtr_add_arg($args, "--defaults-group-suffix=%s", $group_suffix);
    client_debug_arg($args, "$client_name-$group_suffix");
  } else {
    client_debug_arg($args, $client_name);
  }
  return mtr_args2str($client_exe, @$args);
}

sub client_arguments_no_grp_suffix($) {
  my $client_name = shift;
  my $client_exe  = mtr_exe_exists("$path_client_bindir/$client_name");
  my $args;
  mtr_init_args(\$args);
  if ($opt_valgrind_clients) {
    valgrind_client_arguments($args, \$client_exe);
  }
  return mtr_args2str($client_exe, @$args);
}

sub mysqlslap_arguments () {
  my $exe = mtr_exe_maybe_exists("$path_client_bindir/mysqlslap");
  if ($exe eq "") {
    # mysqlap was not found

    if (defined $mysql_version_id and $mysql_version_id >= 50100) {
      mtr_error("Could not find the mysqlslap binary");
    }
    return "";    # Don't care about mysqlslap
  }

  my $args;
  mtr_init_args(\$args);
  if ($opt_valgrind_clients) {
    valgrind_client_arguments($args, \$exe);
  }
  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  client_debug_arg($args, "mysqlslap");
  return mtr_args2str($exe, @$args);
}

sub mysqldump_arguments ($) {
  my ($group_suffix) = @_;
  my $exe = mtr_exe_exists("$path_client_bindir/mysqldump");

  my $args;
  mtr_init_args(\$args);
  if ($opt_valgrind_clients) {
    valgrind_client_arguments($args, \$exe);
  }

  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $group_suffix);
  client_debug_arg($args, "mysqldump-$group_suffix");
  return mtr_args2str($exe, @$args);
}

sub mysql_client_test_arguments() {
  my $exe;
  # mysql_client_test executable may _not_ exist
  $exe = mtr_exe_maybe_exists("$path_client_bindir/mysql_client_test");
  return "" unless $exe;

  my $args;
  mtr_init_args(\$args);
  if ($opt_valgrind_mysqltest) {
    valgrind_arguments($args, \$exe);
  }

  mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  mtr_add_arg($args, "--testcase");
  mtr_add_arg($args, "--vardir=$opt_vardir");
  client_debug_arg($args, "mysql_client_test");

  return mtr_args2str($exe, @$args);
}

sub mysqlxtest_arguments() {
  my $exe;
  # mysqlxtest executable may _not_ exist
  $exe = mtr_exe_maybe_exists("$path_client_bindir/mysqlxtest");
  return "" unless $exe;

  my $args;
  mtr_init_args(\$args);

  if ($opt_valgrind_clients) {
    valgrind_client_arguments($args, \$exe);
  }

  if ($opt_debug) {
    mtr_add_arg($args, "--debug=$debug_d:t:i:A,%s/log/%s.trace",
                $path_vardir_trace, "mysqlxtest");
  }

  mtr_add_arg($args, "--port=%d", $mysqlx_baseport);
  return mtr_args2str($exe, @$args);
}

sub mysqlpump_arguments ($) {
  my ($group_suffix) = @_;
  my $exe = mtr_exe_exists("$path_client_bindir/mysqlpump");

  my $args;
  mtr_init_args(\$args);
  if ($opt_valgrind_clients) {
    valgrind_client_arguments($args, \$exe);
  }

  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $group_suffix);
  client_debug_arg($args, "mysqlpump-$group_suffix");
  return mtr_args2str($exe, @$args);
}

sub mysqlbackup_arguments () {
  my $exe =
    mtr_exe_maybe_exists(vs_config_dirs('runtime_output_directory',
                                        'mysqlbackup'
                         ),
                         "$path_client_bindir/mysqlbackup");
  return "" unless $exe;

  my $args;
  mtr_init_args(\$args);
  if ($opt_valgrind_clients) {
    valgrind_client_arguments($args, \$exe);
  }
  return mtr_args2str($exe, @$args);
}

sub mysqlbackup_plugin_dir () {
  my $fnm = find_plugin('mysqlbackup_sbt_test_mms', 'plugin_output_directory');
  return "" unless $fnm;

  return dirname($fnm);
}

# Set environment to be used by childs of this process for things that
# are constant during the whole lifetime of mysql-test-run.
sub find_plugin($$) {
  my ($plugin, $location) = @_;
  my $plugin_filename;

  if (IS_WINDOWS) {
    $plugin_filename = $plugin . ".dll";
  } else {
    $plugin_filename = $plugin . ".so";
  }

  my $lib_plugin =
    mtr_file_exists(vs_config_dirs($location, $plugin_filename),
                    "$basedir/lib/plugin/" . $plugin_filename,
                    "$basedir/lib64/plugin/" . $plugin_filename,
                    "$basedir/lib/mysql/plugin/" . $plugin_filename,
                    "$basedir/lib64/mysql/plugin/" . $plugin_filename,);
  return $lib_plugin;
}

# Read plugin defintions file
sub read_plugin_defs($) {
  my ($defs_file) = @_;
  my $running_debug = 0;

  open(PLUGDEF, '<', $defs_file) or
    mtr_error("Can't read plugin defintions file $defs_file");

  # Need to check if we will be running mysqld-debug
  if ($opt_debug_server) {
    $running_debug = 1 if find_mysqld($basedir) =~ /mysqld-debug/;
  }

  while (<PLUGDEF>) {
    next if /^#/;
    my ($plug_file, $plug_loc, $plug_var, $plug_names) = split;

    # Allow empty lines
    next unless $plug_file;
    mtr_error("Lines in $defs_file must have 3 or 4 items") unless $plug_var;

    # If running debug server, plugins will be in 'debug' subdirectory
    $plug_file = "debug/$plug_file" if $running_debug && !$source_dist;

    my ($plugin) = find_plugin($plug_file, $plug_loc);

    if (!$plugin) {
      ($plugin) = find_plugin($plug_file, "rapid/$plug_loc");
    }

    # Set env. variables that tests may use, set to empty if plugin
    # listed in def. file but not found.
    if ($plugin) {
      $ENV{$plug_var}            = basename($plugin);
      $ENV{ $plug_var . '_DIR' } = dirname($plugin);
      $ENV{ $plug_var . '_OPT' } = "--plugin-dir=" . dirname($plugin);

      if ($plug_names) {
        my $lib_name     = basename($plugin);
        my $load_var     = "--plugin_load=";
        my $load_add_var = "--plugin_load_add=";
        my $semi         = '';

        foreach my $plug_name (split(',', $plug_names)) {
          $load_var     .= $semi . "$plug_name=$lib_name";
          $load_add_var .= $semi . "$plug_name=$lib_name";
          $semi = ';';
        }

        $ENV{ $plug_var . '_LOAD' }     = $load_var;
        $ENV{ $plug_var . '_LOAD_ADD' } = $load_add_var;
      }
    } else {
      $ENV{$plug_var}            = "";
      $ENV{ $plug_var . '_DIR' } = "";
      $ENV{ $plug_var . '_OPT' } = "";
      $ENV{ $plug_var . '_LOAD' }     = "" if $plug_names;
      $ENV{ $plug_var . '_LOAD_ADD' } = "" if $plug_names;
    }
  }
  close PLUGDEF;
}

# Sets a long list of environment variables. Those that begin with
# MYSQL_ are set with the the intent of being used in tests. The
# subroutine is called by each worker thread, since some of the
# MYSQLD_ variables refer to paths which will actually be worker
# specific.
sub environment_setup {
  umask(022);

  my @ld_library_paths;

  if ($path_client_libdir) {
    # Use the --client-libdir passed on commandline
    push(@ld_library_paths, "$path_client_libdir");
  }

  # Plugin settings should no longer be added here, instead place
  # definitions in include/plugin.defs.
  # See comment in that file for details.
  if (@ld_library_paths) {
    $ENV{'LD_LIBRARY_PATH'} = join(":",
            @ld_library_paths,
            $ENV{'LD_LIBRARY_PATH'} ? split(':', $ENV{'LD_LIBRARY_PATH'}) : ());
    mtr_warning("LD_LIBRARY_PATH: $ENV{'LD_LIBRARY_PATH'}");

    $ENV{'DYLD_LIBRARY_PATH'} = join(":",
        @ld_library_paths,
        $ENV{'DYLD_LIBRARY_PATH'} ? split(':', $ENV{'DYLD_LIBRARY_PATH'}) : ());
    mtr_verbose("DYLD_LIBRARY_PATH: $ENV{'DYLD_LIBRARY_PATH'}");
  }
  $ENV{'UMASK'}     = "0660";    # The octal *string*
  $ENV{'UMASK_DIR'} = "0770";    # The octal *string*

  # MySQL tests can produce output in various character sets
  # (especially, ctype_xxx.test). To avoid confusing Perl with output
  # which is incompatible with the current locale settings, we reset
  # the current values of LC_ALL and LC_CTYPE to "C". For details,
  # please see Bug#27636 tests fails if LC_* variables set to *_*.UTF-8
  $ENV{'LC_ALL'}     = "C";
  $ENV{'LC_COLLATE'} = "C";
  $ENV{'LC_CTYPE'}   = "C";

  $ENV{'DEFAULT_MASTER_PORT'} = $mysqld_variables{'port'};
  $ENV{'MYSQL_BINDIR'}        = "$bindir";
  $ENV{'MYSQL_CHARSETSDIR'}   = $path_charsetsdir;
  $ENV{'MYSQL_SHAREDIR'}      = $path_language;
  $ENV{'MYSQL_TEST_DIR'}      = $glob_mysql_test_dir;
  $ENV{'MYSQL_TEST_DIR_ABS'}  = getcwd();
  $ENV{'MYSQL_TMP_DIR'}       = $opt_tmpdir;
  $ENV{'MYSQLTEST_VARDIR'}    = $opt_vardir;
  $ENV{'USE_RUNNING_SERVER'}  = using_extern();

  if (IS_WINDOWS) {
    $ENV{'SECURE_LOAD_PATH'}      = $glob_mysql_test_dir . "\\std_data";
    $ENV{'MYSQL_TEST_LOGIN_FILE'} = $opt_tmpdir . "\\.mylogin.cnf";
    $ENV{'MYSQLTEST_VARDIR_ABS'}  = $opt_vardir;
  } else {
    $ENV{'SECURE_LOAD_PATH'}      = $glob_mysql_test_dir . "/std_data";
    $ENV{'MYSQL_TEST_LOGIN_FILE'} = $opt_tmpdir . "/.mylogin.cnf";
    $ENV{'MYSQLTEST_VARDIR_ABS'}  = abs_path("$opt_vardir");
  }

  # Setup env for NDB
  if ($ndbcluster_enabled) {
    $ENV{'NDB_MGM'} =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ], "ndb_mgm");

    $ENV{'NDB_WAITER'} = $exe_ndb_waiter;

    $ENV{'NDB_RESTORE'} =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ],
                  "ndb_restore");

    $ENV{'NDB_CONFIG'} =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ], "ndb_config");

    $ENV{'NDB_SELECT_ALL'} =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ],
                  "ndb_select_all");

    $ENV{'NDB_DROP_TABLE'} =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ],
                  "ndb_drop_table");

    $ENV{'NDB_DESC'} =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ], "ndb_desc");

    $ENV{'NDB_SHOW_TABLES'} =
      my_find_bin($bindir, [ "runtime_output_directory", "bin" ],
                  "ndb_show_tables");

    my $ndbapi_examples_binary =
      my_find_bin($bindir, [ "storage/ndb/ndbapi-examples", "bin" ],
                  "ndb_ndbapi_simple", NOT_REQUIRED);

    if ($ndbapi_examples_binary) {
      $ENV{'NDB_EXAMPLES_BINARY'} = $ndbapi_examples_binary;
      $ENV{'NDB_EXAMPLES_DIR'}    = dirname($ndbapi_examples_binary);
      mtr_verbose("NDB_EXAMPLES_DIR: $ENV{'NDB_EXAMPLES_DIR'}");
    }

    my $path_ndb_testrun_log = "$opt_vardir/tmp/ndb_testrun.log";
    $ENV{'NDB_TOOLS_OUTPUT'} = $path_ndb_testrun_log;
  }

  # mysql clients
  $ENV{'EXE_MYSQL'}           = $exe_mysql;
  $ENV{'MYSQL'}               = client_arguments("mysql");
  $ENV{'MYSQL_BINLOG'}        = client_arguments("mysqlbinlog");
  $ENV{'MYSQL_CHECK'}         = client_arguments("mysqlcheck");
  $ENV{'MYSQL_CLIENT_TEST'}   = mysql_client_test_arguments();
  $ENV{'MYSQL_DUMP'}          = mysqldump_arguments(".1");
  $ENV{'MYSQL_DUMP_SLAVE'}    = mysqldump_arguments(".2");
  $ENV{'MYSQL_IMPORT'}        = client_arguments("mysqlimport");
  $ENV{'MYSQL_PUMP'}          = mysqlpump_arguments(".1");
  $ENV{'MYSQL_SHOW'}          = client_arguments("mysqlshow");
  $ENV{'MYSQL_SLAP'}          = mysqlslap_arguments();
  $ENV{'MYSQL_SLAVE'}         = client_arguments("mysql", ".2");
  $ENV{'MYSQL_SSL_RSA_SETUP'} = $exe_mysql_ssl_rsa_setup;
  $ENV{'MYSQL_UPGRADE'}       = client_arguments("mysql_upgrade");
  $ENV{'MYSQLADMIN'}          = native_path($exe_mysqladmin);
  $ENV{'MYSQLXTEST'}          = mysqlxtest_arguments();
  $ENV{'PATH_CONFIG_FILE'}    = $path_config_file;

  $ENV{'MYSQLBACKUP'} = mysqlbackup_arguments()
    unless $ENV{'MYSQLBACKUP'};
  $ENV{'MYSQLBACKUP_PLUGIN_DIR'} = mysqlbackup_plugin_dir()
    unless $ENV{'MYSQLBACKUP_PLUGIN_DIR'};
  $ENV{'MYSQL_CONFIG_EDITOR'} =
    client_arguments_no_grp_suffix("mysql_config_editor");
  $ENV{'MYSQL_SECURE_INSTALLATION'} =
    "$path_client_bindir/mysql_secure_installation";

  my $exe_mysqld = find_mysqld($basedir);
  $ENV{'MYSQLD'} = $exe_mysqld;

  my $extra_opts = join(" ", @opt_extra_mysqld_opt);
  $ENV{'MYSQLD_CMD'} =
    "$exe_mysqld --defaults-group-suffix=.1 " .
    "--defaults-file=$path_config_file $extra_opts";

  # bug25714 executable may _not_ exist in some versions, test using
  # it should be skipped.
  my $exe_bug25714 = mtr_exe_maybe_exists("$path_client_bindir/bug25714");
  $ENV{'MYSQL_BUG25714'} = native_path($exe_bug25714);

  # Get the bin dir
  $ENV{'MYSQL_BIN_PATH'} = native_path($bindir);

  # mysql_fix_privilege_tables.sql
  my $file_mysql_fix_privilege_tables =
    mtr_file_exists("$basedir/scripts/mysql_fix_privilege_tables.sql",
                    "$basedir/share/mysql_fix_privilege_tables.sql",
                    "$basedir/share/mysql/mysql_fix_privilege_tables.sql",
                    "$bindir/scripts/mysql_fix_privilege_tables.sql",
                    "$bindir/share/mysql_fix_privilege_tables.sql",
                    "$bindir/share/mysql/mysql_fix_privilege_tables.sql");
  $ENV{'MYSQL_FIX_PRIVILEGE_TABLES'} = $file_mysql_fix_privilege_tables;

  # my_print_defaults
  my $exe_my_print_defaults =
    mtr_exe_exists("$path_client_bindir/my_print_defaults");
  $ENV{'MYSQL_MY_PRINT_DEFAULTS'} = native_path($exe_my_print_defaults);

  # Setup env so childs can execute innochecksum
  my $exe_innochecksum = mtr_exe_exists("$path_client_bindir/innochecksum");
  $ENV{'INNOCHECKSUM'} = native_path($exe_innochecksum);
  if ($opt_valgrind_clients) {
    my $args;
    mtr_init_args(\$args);
    valgrind_client_arguments($args, \$exe_innochecksum);
    $ENV{'INNOCHECKSUM'} = mtr_args2str($exe_innochecksum, @$args);
  }

  # Setup env so childs can execute ibd2sdi
  my $exe_ibd2sdi = mtr_exe_exists("$path_client_bindir/ibd2sdi");
  $ENV{'IBD2SDI'} = native_path($exe_ibd2sdi);

  if ($opt_valgrind_clients) {
    my $args;
    mtr_init_args(\$args);
    valgrind_client_arguments($args, \$exe_ibd2sdi);
    $ENV{'IBD2SDI'} = mtr_args2str($exe_ibd2sdi, @$args);
  }

  # Setup env so childs can execute myisampack and myisamchk
  $ENV{'MYISAMCHK'} =
    native_path(mtr_exe_exists("$path_client_bindir/myisamchk"));
  $ENV{'MYISAMPACK'} =
    native_path(mtr_exe_exists("$path_client_bindir/myisampack"));

  # mysqld_safe
  my $mysqld_safe = mtr_pl_maybe_exists("$bindir/scripts/mysqld_safe") ||
    mtr_pl_maybe_exists("$path_client_bindir/mysqld_safe");

  if ($mysqld_safe) {
    $ENV{'MYSQLD_SAFE'} = $mysqld_safe;
  }

  # mysqldumpslow
  my $mysqldumpslow = mtr_pl_maybe_exists("$bindir/scripts/mysqldumpslow") ||
    mtr_pl_maybe_exists("$path_client_bindir/mysqldumpslow");

  if ($mysqldumpslow) {
    $ENV{'MYSQLDUMPSLOW'} = $mysqldumpslow;
  }

  # perror
  my $exe_perror = mtr_exe_exists("$path_client_bindir/perror");
  $ENV{'MY_PERROR'} = native_path($exe_perror);

  # mysql_tzinfo_to_sql is not used on Windows, but vs_config_dirs
  # is needed when building with Xcode on OSX.
  my $exe_mysql_tzinfo_to_sql =
    mtr_exe_exists("$path_client_bindir/mysql_tzinfo_to_sql");
  $ENV{'MYSQL_TZINFO_TO_SQL'} = native_path($exe_mysql_tzinfo_to_sql);

  # lz4_decompress
  my $exe_lz4_decompress =
    mtr_exe_maybe_exists("$path_client_bindir/lz4_decompress");
  $ENV{'LZ4_DECOMPRESS'} = native_path($exe_lz4_decompress);

  # zlib_decompress
  my $exe_zlib_decompress =
    mtr_exe_maybe_exists("$path_client_bindir/zlib_decompress");
  $ENV{'ZLIB_DECOMPRESS'} = native_path($exe_zlib_decompress);

  # Create an environment variable to make it possible
  # to detect that valgrind is being used from test cases
  $ENV{'VALGRIND_TEST'} = $opt_valgrind;

  # Create an environment variable to make it possible
  # to detect if valgrind is being used on the server
  # for test cases
  $ENV{'VALGRIND_SERVER_TEST'} = $opt_valgrind_mysqld;

  # Ask UBSAN to print stack traces
  $ENV{'UBSAN_OPTIONS'} = "print_stacktrace=1" if $opt_sanitize;

  # Make sure LeakSanitizer exits if leaks are found
  $ENV{'LSAN_OPTIONS'} = "exitcode=42" if $opt_sanitize;

  # Add dir of this perl to aid mysqltest in finding perl
  my $perldir = dirname($^X);
  my $pathsep = ":";
  $pathsep = ";" if IS_WINDOWS && !IS_CYGWIN;
  $ENV{'PATH'} = "$ENV{'PATH'}" . $pathsep . $perldir;
}

sub remove_vardir_subs() {
  foreach my $sdir (glob("$opt_vardir/*")) {
    mtr_verbose("Removing subdir $sdir");
    rmtree($sdir);
  }
}

# Remove var and any directories in var/ created by previous tests
sub remove_stale_vardir () {
  mtr_report("Removing old var directory...");

  mtr_error("No, don't remove the vardir when running with --extern")
    if using_extern();

  mtr_verbose("opt_vardir: $opt_vardir");
  if ($opt_vardir eq $default_vardir) {
    # Running with "var" in mysql-test dir
    if (-l $opt_vardir) {
      # var is a symlink
      if ($opt_mem) {
        # Remove the directory which the link points at
        mtr_verbose("Removing " . readlink($opt_vardir));
        rmtree(readlink($opt_vardir));

        # Remove the "var" symlink
        mtr_verbose("unlink($opt_vardir)");
        unlink($opt_vardir);
      } else {
        # Some users creates a soft link in mysql-test/var to another area
        # - allow it, but remove all files in it
        mtr_report(" - WARNING: Using the 'mysql-test/var' symlink");

        # Make sure the directory where it points exist
        mtr_error("The destination for symlink $opt_vardir does not exist")
          if !-d readlink($opt_vardir);

        remove_vardir_subs();
      }
    } else {
      # Remove the entire "var" dir
      mtr_verbose("Removing $opt_vardir/");
      rmtree("$opt_vardir/");
    }

    if ($opt_mem) {
      # A symlink from var/ to $opt_mem will be set up remove the
      # $opt_mem dir to assure the symlink won't point at an old
      # directory.
      mtr_verbose("Removing $opt_mem");
      rmtree($opt_mem);
    }

  } else {
    # Running with "var" in some other place. Remove the var/ dir in
    # mysql-test dir if any this could be an old symlink that shouldn't
    # be there.
    mtr_verbose("Removing $default_vardir");
    rmtree($default_vardir);

    # Remove the "var" dir
    mtr_verbose("Removing $opt_vardir/");
    rmtree("$opt_vardir/");
  }
  # Remove the "tmp" dir
  mtr_verbose("Removing $opt_tmpdir/");
  rmtree("$opt_tmpdir/");
}

# Create var and the directories needed in var
sub setup_vardir() {
  mtr_report("Creating var directory '$opt_vardir'...");

  if ($opt_vardir eq $default_vardir) {
    # Running with "var" in mysql-test dir
    if (-l $opt_vardir) {
      #  It's a symlink, make sure the directory where it points exist
      mtr_error("The destination for symlink $opt_vardir does not exist")
        if !-d readlink($opt_vardir);
    } elsif ($opt_mem) {
      # Runinng with "var" as a link to some "memory" location, normally tmpfs
      mtr_verbose("Creating $opt_mem");
      mkpath($opt_mem);

      mtr_report(" - symlinking 'var' to '$opt_mem'");
      symlink($opt_mem, $opt_vardir);
    }
  }

  if (!-d $opt_vardir) {
    mtr_verbose("Creating $opt_vardir");
    mkpath($opt_vardir);
  }

  # Ensure a proper error message if vardir couldn't be created
  unless (-d $opt_vardir and -w $opt_vardir) {
    mtr_error("Writable 'var' directory is needed, use the " .
              "'--vardir=<path>' option");
  }

  mkpath("$opt_vardir/log");
  mkpath("$opt_vardir/run");

  # Create var/tmp and tmp - they might be different
  mkpath("$opt_vardir/tmp");
  mkpath($opt_tmpdir) if ($opt_tmpdir ne "$opt_vardir/tmp");

  # On some operating systems, there is a limit to the length of a
  # UNIX domain socket's path far below PATH_MAX. Don't allow that
  # to happen.
  my $res = check_socket_path_length("$opt_tmpdir/mysql_testsocket.sock",
                                     $opt_parallel);
  if ($res) {
    mtr_error("Socket path '$opt_tmpdir' too long, it would be ",
              "truncated and thus not possible to use for connection to ",
              "MySQL Server. Set a shorter with --tmpdir=<path> option");
  }

  # Copy all files from std_data into var/std_data
  # and make them world readable
  copytree("$glob_mysql_test_dir/std_data", "$opt_vardir/std_data", "0022");

  # Remove old log files
  foreach my $name (glob("r/*.progress r/*.log r/*.warnings")) {
    unlink($name);
  }
}

# Check if running as root i.e a file can be read regardless what mode
# we set it to.
sub check_running_as_root () {
  my $test_file = "$opt_vardir/test_running_as_root.txt";
  mtr_tofile($test_file, "MySQL");
  chmod(oct("0000"), $test_file);

  my $result = "";
  if (open(FILE, "<", $test_file)) {
    $result = join('', <FILE>);
    close FILE;
  }

  # Some filesystems( for example CIFS) allows reading a file although
  # mode was set to 0000, but in that case a stat on the file will not
  # return 0000.
  my $file_mode = (stat($test_file))[2] & 07777;

  mtr_verbose("result: $result, file_mode: $file_mode");
  if ($result eq "MySQL" && $file_mode == 0) {
    mtr_warning(
      "running this script as _root_ will cause some " . "tests to be skipped");
    $ENV{'MYSQL_TEST_ROOT'} = "YES";
  }

  chmod(oct("0755"), $test_file);
  unlink($test_file);
}

sub check_ssl_support ($) {
  my $mysqld_variables = shift;

  if ($opt_skip_ssl) {
    mtr_report(" - skipping SSL");
    $opt_ssl_supported = 0;
    $opt_ssl           = 0;
    return;
  }

  if (!($mysqld_variables->{'ssl'} || $mysqld_variables->{'have_ssl'})) {
    if ($opt_ssl) {
      mtr_error("Couldn't find support for SSL");
      return;
    }
    mtr_report(" - skipping SSL, mysqld not compiled with SSL");
    $opt_ssl_supported = 0;
    $opt_ssl           = 0;
    return;
  }
  mtr_report(" - SSL connections supported");
  $opt_ssl_supported = 1;
}

sub check_debug_support ($) {
  my $mysqld_variables = shift;

  if (!$mysqld_variables->{'debug'}) {
    $debug_compiled_binaries = 0;

    if ($opt_debug) {
      mtr_error("Can't use --debug, binary does not support it");
    }
    if ($opt_debug_server) {
      mtr_warning("Ignoring --debug-server, binary does not support it");
    }
    return;
  }
  mtr_report(" - binaries are debug compiled");
  $debug_compiled_binaries = 1;
}

# Helper function to handle configuration-based subdirectories which
# Visual Studio or XCode uses for storing binaries.  If opt_vs_config
# is set, this returns a path based on that setting; if not, it
# returns paths for the default /release/ and /debug/ subdirectories.
# $exe can be undefined, if the directory itself will be used
sub vs_config_dirs ($$) {
  my ($path_part, $exe) = @_;

  $exe = "" if not defined $exe;

  if (IS_WINDOWS or IS_MAC) {
    if ($opt_vs_config) {
      return ("$bindir/$path_part/$opt_vs_config/$exe");
    }

    return ("$bindir/$path_part/Release/$exe",
            "$bindir/$path_part/RelWithDebinfo/$exe",
            "$bindir/$path_part/Debug/$exe",
            "$bindir/$path_part/$exe");
  }

  return ("$bindir/$path_part/$exe");
}

sub check_ndbcluster_support ($) {
  my $mysqld_variables = shift;

  my $ndbcluster_supported = 0;
  if ($mysqld_variables{'ndb-connectstring'}) {
    $ndbcluster_supported = 1;
  }

  if ($opt_skip_ndbcluster && $opt_include_ndbcluster) {
    # User is ambivalent. Theoretically the arg which was
    # given last on command line should win, but that order is
    # unknown at this time.
    mtr_error("Ambigous command, both --include-ndbcluster " .
              " and --skip-ndbcluster was specified");
  }

  # Check if this is MySQL Cluster, ie. mysql version string ends
  # with -ndb-Y.Y.Y[-status]
  if (defined $mysql_version_extra &&
      $mysql_version_extra =~ /-ndb-([0-9]*)\.([0-9]*)\.([0-9]*)/) {
    # MySQL Cluster tree
    mtr_report(" - MySQL Cluster detected");

    if ($opt_skip_ndbcluster) {
      mtr_report(" - skipping ndbcluster(--skip-ndbcluster)");
      return;
    }

    if (!$ndbcluster_supported) {
      # MySQL Cluster tree, but mysqld was not compiled with
      # ndbcluster -> fail unless --skip-ndbcluster was used
      mtr_error("This is MySQL Cluster but mysqld does not " .
                "support ndbcluster. Use --skip-ndbcluster to " .
                "force mtr to run without it.");
    }

    # mysqld was compiled with ndbcluster -> auto enable
  } else {
    # Not a MySQL Cluster tree
    if (!$ndbcluster_supported) {
      if ($opt_include_ndbcluster) {
        mtr_error("Could not detect ndbcluster support " .
                  "requested with --[ndb|include-ndbcluster]");
      }

      # Silently skip, mysqld was compiled without ndbcluster
      # which is the default case
      return;
    }

    if ($opt_skip_ndbcluster) {
      # Compiled with ndbcluster but ndbcluster skipped
      mtr_report(" - skipping ndbcluster(--skip-ndbcluster)");
      return;
    }

    if (!$opt_include_ndbcluster) {
      # Add only the test suite for ndbcluster integration check
      mtr_report(" - enabling ndbcluster(for integration checks)");
      $ndbcluster_enabled = 1;
      $DEFAULT_SUITES .= ",ndbcluster";
      return;
    }
  }

  mtr_report(" - enabling ndbcluster");
  $ndbcluster_enabled = 1;
  # Add MySQL Cluster test suites
  $DEFAULT_SUITES .=
    ",ndb,ndb_binlog,rpl_ndb,ndb_rpl,ndb_memcache,ndbcluster,ndb_ddl,gcol_ndb";
  return;
}

sub ndbcluster_wait_started($$) {
  my $cluster              = shift;
  my $ndb_waiter_extra_opt = shift;
  my $path_waitlog = join('/', $opt_vardir, $cluster->name(), "ndb_waiter.log");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $cluster->suffix());
  mtr_add_arg($args, "--timeout=%d",               $opt_start_timeout);

  if ($ndb_waiter_extra_opt) {
    mtr_add_arg($args, "$ndb_waiter_extra_opt");
  }

  # Start the ndb_waiter which will connect to the ndb_mgmd
  # and poll it for state of the ndbd's, will return when
  # all nodes in the cluster is started
  my $res = My::SafeProcess->run(name   => "ndb_waiter " . $cluster->name(),
                                 path   => $exe_ndb_waiter,
                                 args   => \$args,
                                 output => $path_waitlog,
                                 error  => $path_waitlog,
                                 append => 1,);

  # Check that ndb_mgmd(s) are still alive
  foreach my $ndb_mgmd (in_cluster($cluster, ndb_mgmds())) {
    my $proc = $ndb_mgmd->{proc};
    if (!$proc->wait_one(0)) {
      mtr_warning("$proc died");
      return 2;
    }
  }

  # Check that all started ndbd(s) are still alive
  foreach my $ndbd (in_cluster($cluster, ndbds())) {
    my $proc = $ndbd->{proc};
    next unless defined $proc;
    if (!$proc->wait_one(0)) {
      mtr_warning("$proc died");
      return 3;
    }
  }

  if ($res) {
    mtr_verbose("ndbcluster_wait_started failed");
    return 1;
  }
  return 0;
}

sub ndbcluster_dump($) {
  my ($cluster) = @_;

  print "\n== Dumping cluster log files\n\n";

  # ndb_mgmd(s)
  foreach my $ndb_mgmd (in_cluster($cluster, ndb_mgmds())) {
    my $datadir = $ndb_mgmd->value('DataDir');

    # Should find ndb_<nodeid>_cluster.log and ndb_mgmd.log
    foreach my $file (glob("$datadir/ndb*.log")) {
      print "$file:\n";
      mtr_printfile("$file");
      print "\n";
    }
  }

  # ndb(s)
  foreach my $ndbd (in_cluster($cluster, ndbds())) {
    my $datadir = $ndbd->value('DataDir');

    # Should find ndbd.log
    foreach my $file (glob("$datadir/ndbd.log")) {
      print "$file:\n";
      mtr_printfile("$file");
      print "\n";
    }
  }
}

sub ndb_mgmd_wait_started($) {
  my ($cluster) = @_;

  my $retries = 100;
  while ($retries) {
    my $result = ndbcluster_wait_started($cluster, "--no-contact");
    if ($result == 0) {
      # ndb_mgmd is started
      mtr_verbose("ndb_mgmd is started");
      return 0;
    } elsif ($result > 1) {
      mtr_warning("Cluster process failed while waiting for start");
      return $result;
    }

    mtr_milli_sleep(100);
    $retries--;
  }

  return 1;
}

sub ndb_mgmd_stop {
  my $ndb_mgmd = shift or die "usage: ndb_mgmd_stop(<ndb_mgmd>)";

  my $host = $ndb_mgmd->value('HostName');
  my $port = $ndb_mgmd->value('PortNumber');
  mtr_verbose("Stopping cluster '$host:$port'");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--ndb-connectstring=%s:%s", $host, $port);
  mtr_add_arg($args, "-e");
  mtr_add_arg($args, "shutdown");

  My::SafeProcess->run(name   => "ndb_mgm shutdown $host:$port",
                       path   => $exe_ndb_mgm,
                       args   => \$args,
                       output => "/dev/null",);
}

sub ndb_mgmd_start ($$) {
  my ($cluster, $ndb_mgmd) = @_;

  mtr_verbose("ndb_mgmd_start");

  my $dir = $ndb_mgmd->value("DataDir");
  mkpath($dir) unless -d $dir;

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $cluster->suffix());
  mtr_add_arg($args, "--mycnf");
  mtr_add_arg($args, "--nodaemon");
  mtr_add_arg($args, "--configdir=%s",             "$dir");

  my $path_ndb_mgmd_log = "$dir/ndb_mgmd.log";

  $ndb_mgmd->{'proc'} =
    My::SafeProcess->new(name     => $ndb_mgmd->after('cluster_config.'),
                         path     => $exe_ndb_mgmd,
                         args     => \$args,
                         output   => $path_ndb_mgmd_log,
                         error    => $path_ndb_mgmd_log,
                         append   => 1,
                         verbose  => $opt_verbose,
                         shutdown => sub { ndb_mgmd_stop($ndb_mgmd) },);
  mtr_verbose("Started $ndb_mgmd->{proc}");

  # FIXME Should not be needed
  # Unfortunately the cluster nodes will fail to start
  # if ndb_mgmd has not started properly
  if (ndb_mgmd_wait_started($cluster)) {
    mtr_warning("Failed to wait for start of ndb_mgmd");
    return 1;
  }

  return 0;
}

sub ndbd_stop {
  # Intentionally left empty, ndbd nodes will be shutdown
  # by sending "shutdown" to ndb_mgmd
}

sub ndbd_start {
  my ($cluster, $ndbd) = @_;

  mtr_verbose("ndbd_start");

  my $dir = $ndbd->value("DataDir");
  mkpath($dir) unless -d $dir;

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $cluster->suffix());
  mtr_add_arg($args, "--nodaemon");

  # > 5.0 { 'character-sets-dir' => \&fix_charset_dir },

  my $exe = $exe_ndbd;
  if ($exe_ndbmtd) {
    if ($ENV{MTR_NDBMTD}) {
      # ndbmtd forced by env var MTR_NDBMTD
      $exe = $exe_ndbmtd;
    }
    if (($exe_ndbmtd_counter++ % 2) == 0) {
      # Use ndbmtd every other time
      $exe = $exe_ndbmtd;
    }
  }

  my $path_ndbd_log = "$dir/ndbd.log";
  my $proc = My::SafeProcess->new(name     => $ndbd->after('cluster_config.'),
                                  path     => $exe,
                                  args     => \$args,
                                  output   => $path_ndbd_log,
                                  error    => $path_ndbd_log,
                                  append   => 1,
                                  verbose  => $opt_verbose,
                                  shutdown => sub { ndbd_stop($ndbd) },);
  mtr_verbose("Started $proc");

  $ndbd->{proc} = $proc;

  return;
}

# Start memcached with the special ndb_engine.so plugin
# making it use NDB as backend.
sub memcached_start {
  my ($cluster, $memcached) = @_;

  my $name = $memcached->name();
  mtr_verbose("memcached_start '$name'");

  # Clear env used by include/have_memcached.inc
  $ENV{'NDB_MEMCACHED_STARTED'} = 0;

  # Look for the ndb_engine.so memcache plugin
  my $found_so = my_find_file(
    $bindir,
    [ "storage/ndb/memcache",    # source or build
      "lib", "lib64"
    ],                           # install
    "ndb_engine.so",
    NOT_REQUIRED);

  if ($found_so eq "") {
    # The ndb_engine memcache plugin is not a mandatory component,
    # silently skip to start memcached if it's not found.
    mtr_verbose("Could not find the ndb_engine memcache plugin");
    return;
  }
  mtr_verbose("Found memcache plugin: '$found_so'");

  # Look for the generated perl script which tells location of memcached etc.
  my $found_perl_source = my_find_file(
    $bindir,
    [ "storage/ndb/memcache",    # source
      "mysql-test/lib",          # install
      "share/mysql-test/lib"
    ],                           # install
    "memcached_path.pl");
  mtr_verbose("Found memcache script: '$found_perl_source'");

  # Source the found perl script which tells location of memcached etc.
  require "$found_perl_source";

  if (!memcached_is_available()) {
    mtr_error("Memcached not available.");
  }
  my $exe = "";
  if (memcached_is_bundled()) {
    # The bundled memcached has been built and made part of the package,
    # find where it ended up and use it.
    $exe = my_find_bin($bindir,
                       [ "libexec", "sbin",
                         "bin",     "storage/ndb/memcache/extra/memcached"
                       ],
                       "memcached");
    mtr_verbose("Found bundled memcached '$exe'");
  } else {
    # External memcached has been used to build ndb_engine, So the path
    # to that memcached has been hardcoded in memcached_path.pl, use that
    # path. This requires same machine as build or memcached also
    # installed in same location as when it was built.
    $exe = get_memcached_exe_path();
    if ($exe eq "" or !-x $exe) {
      mtr_error("Failed to find memcached binary '$exe'");
    }
    mtr_verbose("Using memcached binary '$exe'");
  }

  my $args;
  mtr_init_args(\$args);

  # TCP port number to listen on
  mtr_add_arg($args, "-p %d", $memcached->value('port'));

  # Max simultaneous connections
  mtr_add_arg($args, "-c %d", $memcached->value('max_connections'));

  # Load engine as storage engine, ie. /path/ndb_engine.so
  mtr_add_arg($args, "-E");
  mtr_add_arg($args, $found_so);

  # Config options for loaded storage engine
  {
    my @opts;
    push(@opts, "connectstring=" . $memcached->value('ndb_connectstring'));
    push(@opts, $memcached->if_exist("options"));
    mtr_add_arg($args, "-e");
    mtr_add_arg($args, join(";", @opts));
  }

  if ($opt_gdb) {
    gdb_arguments(\$args, \$exe, "memcached");
  }

  my $proc = My::SafeProcess->new(name    => $name,
                                  path    => $exe,
                                  args    => \$args,
                                  output  => "$opt_vardir/log/$name.out",
                                  error   => "$opt_vardir/log/$name.out",
                                  append  => 1,
                                  verbose => $opt_verbose,);
  mtr_verbose("Started $proc");

  $memcached->{proc} = $proc;

  # Set env used by include/have_memcached.inc
  $ENV{'NDB_MEMCACHED_STARTED'} = 1;

  return;
}

sub memcached_load_metadata($) {
  my $cluster = shift;

  foreach my $mysqld (mysqlds()) {
    if (-d $mysqld->value('datadir') . "/" . "ndbmemcache") {
      mtr_verbose("skipping memcache metadata (already stored)");
      return;
    }
  }

  my $sql_script = my_find_file(
    $bindir,
    [ "share/mysql/memcache-api",    # RPM install
      "share/memcache-api",          # Other installs
      "scripts"                      # Build tree
    ],
    "ndb_memcache_metadata.sql",
    NOT_REQUIRED);
  mtr_verbose("memcached_load_metadata: '$sql_script'");

  if (-f $sql_script) {
    my $args;
    mtr_init_args(\$args);
    mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
    mtr_add_arg($args, "--defaults-group-suffix=%s", $cluster->suffix());
    mtr_add_arg($args, "--connect-timeout=20");

    my $res =
      My::SafeProcess->run(name   => "ndbmemcache config loader",
                           path   => $exe_mysql,
                           args   => \$args,
                           input  => $sql_script,
                           output => "$opt_vardir/log/memcache_config.log",
                           error  => "$opt_vardir/log/memcache_config.log");

    if ($res != 0) {
      mtr_error("Could not load ndb_memcache_metadata.sql file");
    }
  }
}

sub ndbcluster_start ($) {
  my $cluster = shift;

  mtr_verbose("ndbcluster_start '" . $cluster->name() . "'");

  foreach my $ndb_mgmd (in_cluster($cluster, ndb_mgmds())) {
    next if started($ndb_mgmd);
    ndb_mgmd_start($cluster, $ndb_mgmd);
  }

  foreach my $ndbd (in_cluster($cluster, ndbds())) {
    next if started($ndbd);
    ndbd_start($cluster, $ndbd);
  }

  return 0;
}

sub create_config_file_for_extern {
  my %opts = (socket   => '/tmp/mysqld.sock',
              port     => 3306,
              user     => $opt_user,
              password => '',
              @_);

  mtr_report("Creating my.cnf file for extern server...");
  my $F = IO::File->new($path_config_file, "w") or
    mtr_error("Can't write to $path_config_file: $!");

  print $F "[client]\n";
  while (my ($option, $value) = each(%opts)) {
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

  # Close the file
  $F = undef;
}

# Kill processes left from previous runs, normally there should be
# none so make sure to warn if there is one.
sub kill_leftovers ($) {
  my $rundir = shift;
  return unless (-d $rundir);

  mtr_report("Checking leftover processes...");

  # Scan the "run" directory for process id's to kill
  opendir(RUNDIR, $rundir) or
    mtr_error("kill_leftovers, can't open dir \"$rundir\": $!");

  while (my $elem = readdir(RUNDIR)) {
    # Only read pid from files that end with .pid
    if ($elem =~ /.*[.]pid$/) {
      my $pidfile = "$rundir/$elem";
      next unless -f $pidfile;
      my $pid = mtr_fromfile($pidfile);
      unlink($pidfile);

      unless ($pid =~ /^(\d+)/) {
        # The pid was not a valid number
        mtr_warning("Got invalid pid '$pid' from '$elem'");
        next;
      }

      mtr_report(" - found old pid $pid in '$elem', killing it...");
      my $ret = kill("KILL", $pid);
      if ($ret == 0) {
        mtr_report("   process did not exist!");
        next;
      }

      my $check_counter = 100;
      while ($ret > 0 and $check_counter--) {
        mtr_milli_sleep(100);
        $ret = kill(0, $pid);
      }
      mtr_report($check_counter ? "   ok!" : "   failed!");
    } else {
      mtr_warning("Found non pid file '$elem' in '$rundir'")
        if -f "$rundir/$elem";
    }
  }
  closedir(RUNDIR);
}

# Check that all the ports that are going to be used are free.
sub check_ports_free ($) {
  my $bthread  = shift;
  my $portbase = $bthread * 10 + 10000;

  for ($portbase .. $portbase + 9) {
    if (mtr_ping_port($_)) {
      mtr_report(" - 'localhost:$_' was not free");
      # One port was not free
      return 0;
    }
  }

  # All ports free
  return 1;
}

sub initialize_servers {
  if (using_extern()) {
    # Running against an already started server, if the specified
    # vardir does not already exist it should be created
    if (!-d $opt_vardir) {
      setup_vardir();
    } else {
      mtr_verbose("No need to create '$opt_vardir' it already exists");
    }
  } else {
    # Kill leftovers from previous run using any pidfiles found in var/run
    kill_leftovers("$opt_vardir/run");

    if (!$opt_start_dirty) {
      remove_stale_vardir();
      setup_vardir();

      mysql_install_db(default_mysqld(), "$opt_vardir/data/");

      # Save the value of MYSQLD_BOOTSTRAP_CMD before a test with bootstrap
      # options in the opt file runs, so that its original value is restored
      # later.
      $initial_bootstrap_cmd = $ENV{'MYSQLD_BOOTSTRAP_CMD'};
    }
  }
}

# Remove all newline characters expect after semicolon
sub sql_to_bootstrap {
  my ($sql) = @_;

  my @lines     = split(/\n/, $sql);
  my $result    = "\n";
  my $delimiter = ';';

  foreach my $line (@lines) {
    # Change current delimiter if line starts with "delimiter"
    if ($line =~ /^delimiter (.*)/) {
      my $new = $1;
      # Remove old delimiter from end of new
      $new =~ s/\Q$delimiter\E$//;
      $delimiter = $new;
      # No need to add the delimiter to result
      next;
    }

    # Add newline if line ends with $delimiter and convert the current
    # delimiter to semicolon
    if ($line =~ /\Q$delimiter\E$/) {
      $line =~ s/\Q$delimiter\E$/;/;
      $result .= "$line\n";
      next;
    }

    # Remove comments starting with '--'
    next if ($line =~ /^\s*--/);

    # Replace @HOSTNAME with localhost
    $line =~ s/\'\@HOSTNAME\@\'/localhost/;

    # Default, just add the line without newline but with a space
    # as separator
    $result .= "$line ";
  }

  return $result;
}

sub default_mysqld {
  # Generate new config file from template
  my $config =
    My::ConfigFactory->new_config({ basedir       => $basedir,
                                    testdir       => $glob_mysql_test_dir,
                                    template_path => "include/default_my.cnf",
                                    vardir        => $opt_vardir,
                                    tmpdir        => $opt_tmpdir,
                                    baseport      => 0,
                                    user          => $opt_user,
                                    password      => '',
                                  });

  my $mysqld = $config->group('mysqld.1') or
    mtr_error("Couldn't find mysqld.1 in default config");
  return $mysqld;
}

# Runs mysqld --initialize (and various other options) to create an
# installed database. It concatenates a number of SQL files and
# provides this as standard input to the mysqld. The database files
# thus created will later be used for a quick "boot" whenever the
# server is restarted.
sub mysql_install_db {
  my ($mysqld, $datadir, $bootstrap_opts) = @_;

  my $install_basedir = $mysqld->value('#mtr_basedir');
  my $install_chsdir  = $mysqld->value('character-sets-dir');
  my $install_datadir = $datadir || $mysqld->value('datadir');

  mtr_report("Installing system database...");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--log-syslog=0");
  mtr_add_arg($args, "--initialize-insecure");
  mtr_add_arg($args, "--loose-skip-ndbcluster");
  mtr_add_arg($args, "--tmpdir=%s", "$opt_vardir/tmp/");
  mtr_add_arg($args, "--core-file");
  mtr_add_arg($args, "--datadir=%s", "$install_datadir");
  mtr_add_arg($args, "--secure-file-priv=%s", "$opt_vardir");

  # Overwrite the buffer size to 24M for certain tests to pass
  mtr_add_arg($args, "--innodb_buffer_pool_size=24M");
  mtr_add_arg($args, "--innodb-log-file-size=5M");

  # Overwrite innodb_autoextend_increment to 8 for reducing the
  # ibdata1 file size.
  mtr_add_arg($args, "--innodb_autoextend_increment=8");

  if ($opt_debug) {
    mtr_add_arg($args, "--debug=$debug_d:t:i:A,%s/log/bootstrap.trace",
                $path_vardir_trace);
  }

  mtr_add_arg($args, "--character-sets-dir=%s", $install_chsdir);

  # Do not generate SSL/RSA certificates automatically.
  mtr_add_arg($args, "--loose-auto_generate_certs=OFF");
  mtr_add_arg($args, "--loose-sha256_password_auto_generate_rsa_keys=OFF");
  mtr_add_arg($args,
              "--loose-caching_sha2_password_auto_generate_rsa_keys=OFF");

  # InnoDB arguments that affect file location and sizes may need to be
  # given to the bootstrap process as well as the server process.
  foreach my $extra_opt (@opt_extra_mysqld_opt) {
    (my $temp_extra_opt = $extra_opt) =~ s/_/-/g;
    if ($temp_extra_opt =~ /--innodb-page-size/ ||
        $temp_extra_opt =~ /--innodb-log-file-size/ ||
        $temp_extra_opt =~ /--innodb-undo-tablespaces/) {
      mtr_add_arg($args, $extra_opt);
    }

    # Plugin arguments need to be given to the bootstrap process as
    # well as the server process.
    if ($extra_opt =~ /--default-authentication-plugin/) {
      mtr_add_arg($args, $extra_opt);
    }
  }

  # Arguments to bootstrap process.
  my $init_file;
  foreach my $extra_opt (@opt_extra_bootstrap_opt) {
    # If init-file is passed, get the file path to merge the contents
    # of the file with bootstrap.sql
    if ($extra_opt =~ /--init[-_]file=(.*)/) {
      $init_file = $1;
    }
    $init_file = get_bld_path($init_file);
    mtr_add_arg($args, $extra_opt);
  }

  # Add bootstrap arguments from the opt file, if any
  push(@$args, @$bootstrap_opts) if $bootstrap_opts;

  # The user can set MYSQLD_BOOTSTRAP to the full path to a mysqld
  # to run a different mysqld during --initialize.
  my $exe_mysqld_bootstrap =
    $ENV{'MYSQLD_BOOTSTRAP'} || find_mysqld($install_basedir);

  # Export MYSQLD_BOOTSTRAP_CMD variable containing <path>/mysqld <args>
  $ENV{'MYSQLD_BOOTSTRAP_CMD'} = "$exe_mysqld_bootstrap " . join(" ", @$args);

  # Export MYSQLD_INSTALL_CMD variable containing <path>/mysqld <args>
  $ENV{'MYSQLD_INSTALL_CMD'} = "$exe_mysqld_bootstrap " . join(" ", @$args);

  # Create the bootstrap.sql file
  my $bootstrap_sql_file = "$opt_vardir/tmp/bootstrap.sql";

  #Add the init-file to --initialize-insecure process
  mtr_add_arg($args, "--init-file=$bootstrap_sql_file");

  if ($opt_boot_gdb || $opt_manual_boot_gdb) {
    gdb_arguments(\$args,          \$exe_mysqld_bootstrap,
                  $mysqld->name(), $bootstrap_sql_file);
  }

  if ($opt_boot_dbx) {
    dbx_arguments(\$args,          \$exe_mysqld_bootstrap,
                  $mysqld->name(), $bootstrap_sql_file);
  }

  if ($opt_boot_ddd) {
    ddd_arguments(\$args,          \$exe_mysqld_bootstrap,
                  $mysqld->name(), $bootstrap_sql_file);
  }

  my $path_sql = my_find_file($install_basedir,
                              [ "mysql", "share/mysql",
                                "share/mysql-" . $mysql_base_version,
                                "share", "scripts"
                              ],
                              "mysql_system_tables.sql",
                              NOT_REQUIRED);

  if (-f $path_sql && -f "include/mtr_test_data_timezone.sql") {
    # Add the offical mysql system tables for a production system.
    mtr_tofile($bootstrap_sql_file, "use mysql;\n");
    mtr_appendfile_to_file($path_sql, $bootstrap_sql_file);

    # Add test data for timezone - this is just a subset, on a real
    # system these tables will be populated either by mysql_tzinfo_to_sql
    # or by downloading the timezone table package from our website
    mtr_appendfile_to_file("include/mtr_test_data_timezone.sql",
                           $bootstrap_sql_file);
  } else {
    mtr_error(
       "Error: The test_data_timezone.sql not found" . "in working directory.");
  }

  if ($opt_skip_sys_schema) {
    mtr_tofile($bootstrap_sql_file, "DROP DATABASE sys;\n");
  }

  # Update table with better values making it easier to restore when changed
  mtr_tofile(
    $bootstrap_sql_file,
    "UPDATE mysql.tables_priv SET
               timestamp = CURRENT_TIMESTAMP,
               Grantor= 'root\@localhost'
               WHERE USER= 'mysql.session';\n");

  # Make sure no anonymous accounts exists as a safety precaution
  mtr_tofile($bootstrap_sql_file, "DELETE FROM mysql.user where user= '';\n");

  # Create test database
  if (defined $opt_charset_for_testdb) {
    mtr_tofile($bootstrap_sql_file,
               "CREATE DATABASE test CHARACTER SET $opt_charset_for_testdb;\n");
  } else {
    mtr_tofile($bootstrap_sql_file, "CREATE DATABASE test;\n");
  }

  # Create mtr database
  mtr_tofile($bootstrap_sql_file, "CREATE DATABASE mtr;\n");

  mtr_tofile(
    $bootstrap_sql_file,
    "insert into mysql.db values('%','test','','Y','Y','Y','Y','Y',
            'Y','N','Y','Y','Y','Y','Y','Y','Y','Y','N','N','Y','Y'); \n");

  # Inserting in acl table generates a timestamp and conventional way
  # generates a null timestamp.
  mtr_tofile($bootstrap_sql_file,
             "DELETE FROM mysql.proxies_priv where user='root';\n");
  mtr_tofile(
    $bootstrap_sql_file,
    "INSERT INTO mysql.proxies_priv VALUES ('localhost', 'root',
              '', '', TRUE, '', now());\n");

  # Add help tables and data for warning detection and supression
  mtr_tofile($bootstrap_sql_file,
             sql_to_bootstrap(mtr_grab_file("include/mtr_warnings.sql")));

  # Add procedures for checking server is restored after testcase
  mtr_tofile($bootstrap_sql_file,
             sql_to_bootstrap(mtr_grab_file("include/mtr_check.sql")));

  if (defined $init_file) {
    # Append the contents of the init-file to the end of bootstrap.sql
    mtr_tofile($bootstrap_sql_file, "use test;\n");
    mtr_appendfile_to_file($init_file, $bootstrap_sql_file);
  }

  # Set blacklist option early so it works during bootstrap
  $ENV{'TSAN_OPTIONS'} = "suppressions=$basedir/mysql-test/tsan.supp"
    if $opt_sanitize;

  if ($opt_manual_boot_gdb) {
    # The configuration has been set up and user has been prompted for
    # how to start the servers manually in the requested debugger.
    # At this time mtr.pl have no knowledge about the server processes
    # and thus can't wait for them to finish, mtr exits at this point.
    exit(0);
  }

  # Log bootstrap command
  my $path_bootstrap_log = "$opt_vardir/log/bootstrap.log";
  mtr_tofile($path_bootstrap_log,
             "$exe_mysqld_bootstrap " . join(" ", @$args) . "\n");

  my $res = My::SafeProcess->run(name    => "initialize",
                                 path    => $exe_mysqld_bootstrap,
                                 args    => \$args,
                                 input   => $bootstrap_sql_file,
                                 output  => $path_bootstrap_log,
                                 error   => $path_bootstrap_log,
                                 append  => 1,
                                 verbose => $opt_verbose,);

  if ($res != 0) {
    mtr_error("Error executing mysqld --initialize\n" .
              "Could not install system database from $bootstrap_sql_file\n" .
              "see $path_bootstrap_log for errors");
  }

  # Remove the auto.cnf so that a new auto.cnf is generated for master
  # and slaves when the server is restarted
  if (-f "$datadir/auto.cnf") {
    unlink "$datadir/auto.cnf";
  }
}

sub run_testcase_check_skip_test($) {
  my ($tinfo) = @_;

  # If marked to skip, just print out and return. Note that a test case
  # not marked as 'skip' can still be skipped later, because of the test
  # case itself in cooperation with the mysqltest program tells us so.
  if ($tinfo->{'skip'}) {
    mtr_report_test_skipped($tinfo) unless $start_only;
    return 1;
  }

  return 0;
}

sub run_query {
  my ($tinfo, $mysqld, $query) = @_;

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));

  mtr_add_arg($args, "-e %s", $query);

  my $res = My::SafeProcess->run(name   => "run_query -> " . $mysqld->name(),
                                 path   => $exe_mysql,
                                 args   => \$args,
                                 output => '/dev/null',
                                 error  => '/dev/null');

  return $res;
}

sub do_before_run_mysqltest($) {
  my $tinfo = shift;

  # Remove old files produced by mysqltest
  my $base_file =
    mtr_match_extension($tinfo->{result_file}, "result");    # Trim extension

  if (defined $base_file) {
    unlink("$base_file.reject");
    unlink("$base_file.progress");
    unlink("$base_file.log");
    unlink("$base_file.warnings");
  }

  if ($mysql_version_id < 50000) {
    # Set environment variable NDB_STATUS_OK to 1
    # if script decided to run mysqltest cluster _is_ installed ok
    $ENV{'NDB_STATUS_OK'} = "1";
  } elsif ($mysql_version_id < 50100) {
    # Set environment variable NDB_STATUS_OK to YES
    # if script decided to run mysqltest cluster _is_ installed ok
    $ENV{'NDB_STATUS_OK'} = "YES";
  }
}

# Check all server for sideffects. Run include/check-testcase.test via
# start_check_testcase(). It's run before and after each test with
# mode set to "before" or "after". In before mode, it records a result
# file, in after mode it checks against it and cleans up the temporary
# files.
#
# RETURN VALUE
#   0  Ok
#   1  Check failed
#   >1 Fatal errro
sub check_testcase($$) {
  my ($tinfo, $mode) = @_;
  my $tname = $tinfo->{name};

  # Start the mysqltest processes in parallel to save time also makes
  # it possible to wait for any process to exit during the check.
  my %started;
  foreach my $mysqld (mysqlds()) {
    # Skip if server has been restarted with additional options
    if (defined $mysqld->{'proc'} && !exists $mysqld->{'restart_opts'}) {
      my $proc = start_check_testcase($tinfo, $mode, $mysqld);
      $started{ $proc->pid() } = $proc;
    }
  }

  # Return immediately if no check proceess was started
  return 0 unless (keys %started);

  my $timeout = start_timer(check_timeout($tinfo));

  while (1) {
    my $result;
    my $proc = My::SafeProcess->wait_any_timeout($timeout);
    mtr_report("Got $proc");

    if (delete $started{ $proc->pid() }) {
      my $err_file = $proc->user_data();
      my $base_file = mtr_match_extension($err_file, "err");    # Trim extension

      # One check testcase process returned
      my $res = $proc->exit_status();

      if ($res == 0) {
        # Check completed without problem, remove the .err file the
        # check generated
        unlink($err_file);

        # Remove the .result file the check generated
        if ($mode eq 'after') {
          unlink("$base_file.result");
        }

        if (keys(%started) == 0) {
          # All checks completed
          mark_time_used('check');
          return 0;
        }

        # Wait for next process to exit
        next;
      } else {
        if ($mode eq "after" and $res == 1) {
          # Test failed, grab the report mysqltest has created
          my $report = mtr_grab_file($err_file);
          my $message =
            "\nMTR's internal check of the test case '$tname' failed.
This means that the test case does not preserve the state that existed
before the test case was executed.  Most likely the test case did not
do a proper clean-up. It could also be caused by the previous test run
by this thread, if the server wasn't restarted.
This is the diff of the states of the servers before and after the
test case was executed:\n";

          $tinfo->{comment} .= $message;
          $tinfo->{comment} .= $report;

          # Do not grab the log file since the test actually passed
          $tinfo->{logfile} = "";

          # Check failed, mark the test case with that info
          $tinfo->{'check_testcase_failed'} = 1;
          $result = 1;
        } elsif ($res) {
          my $report = mtr_grab_file($err_file);
          $tinfo->{comment} .=
            "Could not execute 'check-testcase' $mode " .
            "testcase '$tname' (res: $res):\n";
          $tinfo->{comment} .= $report;
          $result = 2;
        }

        # Remove the .result file the check generated
        unlink("$base_file.result");
      }
    } elsif ($proc->{timeout}) {
      $tinfo->{comment} .= "Timeout for 'check-testcase' expired after " .
        check_timeout($tinfo) . " seconds";
      $result = 4;
    } else {
      # Unknown process returned, most likley a crash, abort everything
      $tinfo->{comment} =
        "The server $proc crashed while running " .
        "'check testcase $mode test'" .
        get_log_from_proc($proc, $tinfo->{name});
      $result = 3;
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
#   0 OK
#   1 Check failed
sub start_run_one ($$) {
  my ($mysqld, $run) = @_;

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));
  mtr_add_arg($args, "--logdir=%s/tmp",            $opt_vardir);
  mtr_add_arg($args, "--test-file=%s",             "include/$run.test");
  mtr_add_arg($args, "--silent");

  my $name    = "$run-" . $mysqld->name();
  my $errfile = "$opt_vardir/tmp/$name.err";
  my $proc = My::SafeProcess->new(name      => $name,
                                  path      => $exe_mysqltest,
                                  error     => $errfile,
                                  output    => $errfile,
                                  args      => \$args,
                                  user_data => $errfile,
                                  verbose   => $opt_verbose,);

  mtr_verbose("Started $proc");
  return $proc;
}

# Run script on all servers, collect results
#
# RETURN VALUE
#   0 ok
#   1 Failure
sub run_on_all($$) {
  my ($tinfo, $run) = @_;

  # Start the mysqltest processes in parallel to save time also makes
  # it possible to wait for any process to exit during the check and
  # to have a timeout process.
  my %started;
  foreach my $mysqld (mysqlds()) {
    if (defined $mysqld->{'proc'}) {
      my $proc = start_run_one($mysqld, $run);
      $started{ $proc->pid() } = $proc;
    }
  }

  # Return immediately if no check proceess was started
  return 0 unless (keys %started);

  my $timeout = start_timer(check_timeout($tinfo));

  while (1) {
    my $result;
    my $proc = My::SafeProcess->wait_any_timeout($timeout);
    mtr_report("Got $proc");

    if (delete $started{ $proc->pid() }) {
      # One mysqltest process returned
      my $err_file = $proc->user_data();
      my $res      = $proc->exit_status();

      # Append the report from .err file
      $tinfo->{comment} .= " == $err_file ==\n";
      $tinfo->{comment} .= mtr_grab_file($err_file);
      $tinfo->{comment} .= "\n";

      # Remove the .err file
      unlink($err_file);

      if (keys(%started) == 0) {
        # All completed
        return 0;
      }

      # Wait for next process to exit
      next;
    } elsif ($proc->{timeout}) {
      $tinfo->{comment} .= "Timeout for '$run' expired after " .
        check_timeout($tinfo) . " seconds\n";
    } else {
      # Unknown process returned, most likley a crash, abort everything
      $tinfo->{comment} .=
        "The server $proc crashed while running '$run'" .
        get_log_from_proc($proc, $tinfo->{name});
    }

    # Kill any check processes still running
    map($_->kill(), values(%started));

    return 1;
  }
  mtr_error("INTERNAL_ERROR: run_on_all");
}

sub mark_log {
  my ($log, $tinfo) = @_;
  my $log_msg = "CURRENT_TEST: $tinfo->{name}\n";
  mtr_tofile($log, $log_msg);
}

sub find_testcase_skipped_reason($) {
  my ($tinfo) = @_;

  # Set default message
  $tinfo->{'comment'} = "Detected by testcase(no log file)";

  # Open the test log file
  my $F = IO::File->new($path_current_testlog) or return;

  my $reason;
  while (my $line = <$F>) {
    # Look for "reason: <reason for skipping test>"
    if ($line =~ /reason: (.*)/) {
      $reason = $1;
    }
  }

  if (!$reason) {
    mtr_warning(
            "Could not find reason for skipping test in $path_current_testlog");
    $reason = "Detected by testcase(reason unknown) ";
  }
  $tinfo->{'comment'} = $reason;
}

sub find_analyze_request {
  # Open the test log file
  my $F = IO::File->new($path_current_testlog) or
    return;
  my $analyze;

  while (my $line = <$F>) {
    # Look for "reason: <reason for skipping test>"
    if ($line =~ /analyze: (.*)/) {
      $analyze = $1;
    }
  }

  return $analyze;
}

# The test can leave a file in var/tmp/ to signal that all servers
# should be restarted.
sub restart_forced_by_test($) {
  my $file = shift;

  my $restart = 0;
  foreach my $mysqld (mysqlds()) {
    my $datadir            = $mysqld->value('datadir');
    my $force_restart_file = "$datadir/mtr/$file";

    if (-f $force_restart_file) {
      mtr_verbose("Restart of servers forced by test");
      $restart = 1;
      last;
    }
  }
  return $restart;
}

# Return timezone value of tinfo or default value
sub timezone {
  my ($tinfo) = @_;
  return $tinfo->{timezone} || "GMT-3";
}

sub resfile_report_test ($) {
  my $tinfo = shift;
  resfile_new_test();
  resfile_test_info("name",      $tinfo->{name});
  resfile_test_info("variation", $tinfo->{combination})
    if $tinfo->{combination};
  resfile_test_info("start_time", isotime time);
}

# Search the opt file for '--bootstrap' key word. For each instance of
# '--bootstrap', save the option immediately after it into an array so
# that datadir can be reinitialized with those options.
sub find_bootstrap_opts {
  my ($opt_file) = @_;

  my $bootstrap_opt = 0;
  my $bootstrap_opts;

  foreach my $opt (@$opt_file) {
    if ($opt =~ /^--bootstrap=/) {
      $opt =~ s/--bootstrap=//;
      push(@$bootstrap_opts, $opt);
    } elsif ($opt eq "--bootstrap") {
      $bootstrap_opt = 1;
      next;
    } elsif ($bootstrap_opt == 1) {
      push(@$bootstrap_opts, $opt);
      $bootstrap_opt = 0;
    }
  }
  return $bootstrap_opts if defined $bootstrap_opts;
}

# This involves checking if the server needs to be restarted after the
# previous test run by this worker, and stopping and restarting them
# if needed.  Contains some complex logic here to take care of
# unexpected failures, skips, server processes dying but the test
# having indicated it is expected, timeouts etc.
#
# RETURN VALUE
#   0 OK
#   > 0 failure
sub run_testcase ($) {
  my $tinfo = shift;

  my $print_freq = 20;

  mtr_verbose("Running test:", $tinfo->{name});
  resfile_report_test($tinfo) if $opt_resfile;

  # Allow only alpanumerics pluss _ - + . in combination names, or
  # anything beginning with -- (the latter comes from --combination).
  my $combination = $tinfo->{combination};
  if ($combination &&
      $combination !~ /^\w[-\w\.\+]+$/ &&
      $combination !~ /^--/) {
    mtr_error("Combination '$combination' contains illegal characters");
  }

  # Init variables that can change between each test case
  my $timezone = timezone($tinfo);
  $ENV{'TZ'} = $timezone;
  mtr_verbose("Setting timezone: $timezone");

  # If there are bootstrap options in the opt file, add them. On retry,
  # bootstrap_master_opt will already be set, so do not call
  # find_bootstrap_opts again.
  $tinfo->{bootstrap_master_opt} = find_bootstrap_opts($tinfo->{master_opt})
    if (!$tinfo->{bootstrap_master_opt});
  $tinfo->{bootstrap_slave_opt} = find_bootstrap_opts($tinfo->{slave_opt})
    if (!$tinfo->{bootstrap_slave_opt});

  # The keyword "--bootstrap" is passed in the opt file to identify
  # the bootstrap variables. Remove this keyword before sending
  # these options to the server.
  @{ $tinfo->{master_opt} } = grep { !/--bootstrap/ } @{ $tinfo->{master_opt} };
  @{ $tinfo->{slave_opt} }  = grep { !/--bootstrap/ } @{ $tinfo->{slave_opt} };

  if (!using_extern()) {
    my @restart = servers_need_restart($tinfo);
    if (@restart != 0) {
      stop_servers($tinfo, @restart);
    }

    if (started(all_servers()) == 0) {
      # Remove old datadirs
      clean_datadir() unless $opt_start_dirty;

      # Restore old ENV
      while (my ($option, $value) = each(%old_env)) {
        if (defined $value) {
          mtr_verbose("Restoring $option to $value");
          $ENV{$option} = $value;

        } else {
          mtr_verbose("Removing $option");
          delete($ENV{$option});
        }
      }
      %old_env = ();

      mtr_verbose("Generating my.cnf from '$tinfo->{template_path}'");

      # Generate new config file from template
      $config = My::ConfigFactory->new_config(
        { basedir             => $basedir,
          testdir             => $glob_mysql_test_dir,
          template_path       => $tinfo->{template_path},
          extra_template_path => $tinfo->{extra_template_path},
          vardir              => $opt_vardir,
          tmpdir              => $opt_tmpdir,
          baseport            => $baseport,
          mysqlxbaseport      => $mysqlx_baseport,
          #hosts          => [ 'host1', 'host2' ],
          user     => $opt_user,
          password => '',
          ssl      => $opt_ssl_supported,
        });

      # Write the new my.cnf
      $config->save($path_config_file);

      # Remember current config so a restart can occur when a test need
      # to use a different one
      $current_config_name = $tinfo->{template_path};

      # Set variables in the ENV section
      foreach my $option ($config->options_in_group("ENV")) {
        # Save old value to restore it before next time
        $old_env{ $option->name() } = $ENV{ $option->name() };
        mtr_verbose($option->name(), "=", $option->value());
        $ENV{ $option->name() } = $option->value();
      }
    }

    # Write start of testcase to log
    mark_log($path_current_testlog, $tinfo);

    if (start_servers($tinfo)) {
      report_failure_and_restart($tinfo);
      return 1;
    }
  }
  mark_time_used('restart');

  # If '--start' or '--start-dirty' given, stop here to let user manually
  # run tests. If '--wait-all' is also given, do the same, but don't die
  # if one server exits.
  if ($start_only) {
    mtr_print("\nStarted",    started(all_servers()));
    mtr_print("Using config", $tinfo->{template_path});
    mtr_print("Port and socket path for server(s):");

    foreach my $mysqld (mysqlds()) {
      mtr_print($mysqld->name() .
               "  " . $mysqld->value('port') . "  " . $mysqld->value('socket'));
    }

    if ($opt_start_exit) {
      mtr_print("Server(s) started, not waiting for them to finish");
      if (IS_WINDOWS) {
        POSIX::_exit(0);    # exit hangs here in ActiveState Perl
      } else {
        exit(0);
      }
    }

    if ($opt_manual_gdb ||
        $opt_manual_lldb  ||
        $opt_manual_ddd   ||
        $opt_manual_debug ||
        $opt_manual_dbx) {
      # The configuration has been set up and user has been prompted for
      # how to start the servers manually in the requested deugger.
      # At this time mtr.pl have no knowledge about the server processes
      # and thus can't wait for them to finish or antyhing. In order to make
      # it apparent to user what to do next, just print message and hang
      # around until user kills mtr.pl
      mtr_print("User prompted how to start server(s) manually in debugger");
      while (1) {
        mtr_milli_sleep(100);
      }
      exit(0);    # Never reached
    }

    mtr_print("Waiting for server(s) to exit...");

    if ($opt_wait_all) {
      My::SafeProcess->wait_all();
      mtr_print("All servers exited");
      exit(1);
    } else {
      my $proc = My::SafeProcess->wait_any();
      if (grep($proc eq $_, started(all_servers()))) {
        mtr_print("Server $proc died");
        exit(1);
      }
      mtr_print("Unknown process $proc died");
      exit(1);
    }
  }

  my $test_timeout = start_timer(testcase_timeout($tinfo));

  do_before_run_mysqltest($tinfo);

  mark_time_used('admin');

  if ($opt_check_testcases and check_testcase($tinfo, "before")) {
    # Failed to record state of server or server crashed
    report_failure_and_restart($tinfo);

    return 1;
  }

  my $test = start_mysqltest($tinfo);

  # Set only when we have to keep waiting after expectedly died server
  my $keep_waiting_proc = 0;
  my $print_timeout     = start_timer($print_freq * 60);

  while (1) {
    my $proc;
    if ($keep_waiting_proc) {
      # Any other process exited?
      $proc = My::SafeProcess->check_any();
      if ($proc) {
        mtr_verbose("Found exited process $proc");
      } else {
        $proc = $keep_waiting_proc;
        # Also check if timer has expired, if so cancel waiting
        if (has_expired($test_timeout)) {
          $keep_waiting_proc = 0;
        }
      }
    }

    if (!$keep_waiting_proc) {
      if ($test_timeout > $print_timeout) {
        $proc = My::SafeProcess->wait_any_timeout($print_timeout);
        if ($proc->{timeout}) {
          # Print out that the test is still on
          mtr_print("Test still running: $tinfo->{name}");

          # Reset the timer
          $print_timeout = start_timer($print_freq * 60);
          next;
        }
      } else {
        $proc = My::SafeProcess->wait_any_timeout($test_timeout);
      }
    }

    # Will be restored if we need to keep waiting
    $keep_waiting_proc = 0;

    unless (defined $proc) {
      mtr_error("wait_any failed");
    }

    mtr_verbose("Got $proc");
    mark_time_used('test');

    # Was it the test program that exited
    if ($proc eq $test) {
      my $res = $test->exit_status();

      if ($res == 0 and $opt_warnings and check_warnings($tinfo)) {
        # Test case succeeded, but it has produced unexpected warnings,
        # continue in $res == 1
        $res = 1;
        resfile_output($tinfo->{'warnings'}) if $opt_resfile;
      }

      my $check_res;
      my $message =
        "Skip condition should be checked in the beginning of a test case,\n" .
        "before modifying any database objects. Most likely the test case\n" .
        "is skipped with the current server configuration after altering\n" .
        "system status. Please fix the test case to perform the skip\n" .
        "condition check before modifying the system status.";

      if ($res == 0 or $res == 62) {
        if ($tinfo->{'no_result_file'}) {
          # Test case doesn't have it's corresponding result file, marking
          # the test case as failed.
          $tinfo->{comment} =
            "Result file '$tinfo->{'no_result_file'}' doesn't exist.\n" .
            "Either create a result file or disable check-testcases and " .
            "run the test case. Use --nocheck-testcases option to " .
            "disable check-testcases.\n";
          $res = 1;
        }
      } elsif ($res == 1) {
        # Test case has failed, delete 'no_result_file' key and its
        # associated value from the test object to avoid any unknown error.
        delete $tinfo->{'no_result_file'} if $tinfo->{'no_result_file'};
      }

      # Check if check-testcase should be run
      if ($opt_check_testcases) {
        if (($res == 0 and !restart_forced_by_test('force_restart')) or
            ($res == 62 and
             !restart_forced_by_test('force_restart_if_skipped'))
        ) {
          $check_res = check_testcase($tinfo, "after");

          # Test run succeeded but failed in check-testcase, marking
          # the test case as failed.
          if (defined $check_res and $check_res == 1) {
            $tinfo->{comment} .= "\n$message" if ($res == 62);
            $res = 1;
          }
        }
      }

      if ($res == 0) {
        if (restart_forced_by_test('force_restart')) {
          stop_all_servers($opt_shutdown_timeout);
        } elsif ($opt_check_testcases and $check_res) {
          # Test case check failed fatally, probably a server crashed
          report_failure_and_restart($tinfo);
          return 1;
        }
        mtr_report_test_passed($tinfo);
      } elsif ($res == 62) {
        if (defined $check_res and $check_res == 1) {
          # Test case had side effects, not fatal error, just continue
          $tinfo->{check} .= "\n$message";
          stop_all_servers($opt_shutdown_timeout);
          mtr_report("Resuming tests...\n");
          resfile_output($tinfo->{'check'}) if $opt_resfile;
          mtr_report_test_passed($tinfo);
        } else {
          # Testcase itself tell us to skip this one
          $tinfo->{skip_detected_by_test} = 1;

          # Try to get reason from test log file
          find_testcase_skipped_reason($tinfo);
          mtr_report_test_skipped($tinfo);

          # Restart if skipped due to missing perl, it may have had side effects
          if (restart_forced_by_test('force_restart_if_skipped') ||
              $tinfo->{'comment'} =~ /^perl not found/) {
            stop_all_servers($opt_shutdown_timeout);
          }
        }
      } elsif ($res == 65) {
        # Testprogram killed by signal
        $tinfo->{comment} = "testprogram crashed(returned code $res)";
        report_failure_and_restart($tinfo);
      } elsif ($res == 1) {
        # Check if the test tool requests that an analyze script should be run
        my $analyze = find_analyze_request();
        if ($analyze) {
          run_on_all($tinfo, "analyze-$analyze");
        }

        # Wait a bit and see if a server died, if so report that instead
        mtr_milli_sleep(100);
        my $srvproc = My::SafeProcess::check_any();
        if ($srvproc && grep($srvproc eq $_, started(all_servers()))) {
          $proc = $srvproc;
          goto SRVDIED;
        }

        # Test case failure reported by mysqltest
        report_failure_and_restart($tinfo);
      } else {
        # mysqltest failed, probably crashed
        $tinfo->{comment} =
          "mysqltest failed with unexpected return code $res\n";
        report_failure_and_restart($tinfo);
      }

      # Save info from this testcase run to mysqltest.log
      if (-f $path_current_testlog) {
        if ($opt_resfile && $res && $res != 62) {
          resfile_output_file($path_current_testlog);
        }
        mtr_appendfile_to_file($path_current_testlog, $path_testlog);
        unlink($path_current_testlog);
      }

      # Remove testcase .log file produce in var/log/ to save space since
      # relevant part of logfile has already been appended to master log
      {
        my $log_file_name =
          $opt_vardir . "/log/" . $tinfo->{shortname} . ".log";
        if (-e $log_file_name && ($tinfo->{'result'} ne 'MTR_RES_FAILED')) {
          unlink($log_file_name);
        }
      }

      return ($res == 62) ? 0 : $res;

    }

    # Check if it was an expected crash
    my $check_crash = check_expected_crash_and_restart($proc);
    if ($check_crash) {
      # Keep waiting if it returned 2, if 1 don't wait or stop waiting.
      $keep_waiting_proc = 0     if $check_crash == 1;
      $keep_waiting_proc = $proc if $check_crash == 2;
      next;
    }

  SRVDIED:
    # Stop the test case timer
    $test_timeout = 0;

    # Check if it was a server that died
    if (grep($proc eq $_, started(all_servers()))) {
      # Server failed, probably crashed
      $tinfo->{comment} =
        "Server $proc failed during test run" .
        get_log_from_proc($proc, $tinfo->{name});

      # It's not mysqltest that has exited, kill it
      mtr_report("Killing mysqltest pid $test");
      $test->kill();

      report_failure_and_restart($tinfo);
      return 1;
    }

    if (!IS_WINDOWS) {
      # Try to dump core for mysqltest and all servers
      foreach my $proc ($test, started(all_servers())) {
        mtr_print("Trying to dump core for $proc");
        if ($proc->dump_core()) {
          $proc->wait_one(20);
        }
      }
    } else {
      # kill mysqltest process
      $test->kill();

      # Try to dump core for all servers
      foreach my $mysqld (mysqlds()) {
        mtr_print("Trying to dump core for $mysqld->{'proc'}");

        # There is high a risk of MTR hanging by calling external programs
        # like 'cdb' on windows with multi-threaded runs(i.e $parallel > 1).
        # Calling cdb only if parallel value is 1.
        my $call_cdb = 1 if ($opt_parallel == 1);
        $mysqld->{'proc'}->dump_core_windows($mysqld, $call_cdb);
        $mysqld->{'proc'}->wait_one(20);
      }
    }

    # It's not mysqltest that has exited, kill it
    mtr_report("Killing mysqltest pid $test");
    $test->kill();

    # Check if testcase timer expired
    if ($proc->{timeout}) {
      $tinfo->{comment} =
        "Test case timeout after " . testcase_timeout($tinfo) . " seconds\n\n";

      # Fetch mysqltest client callstack information
      if (-e $path_current_testlog) {
        $tinfo->{comment} .= mtr_callstack_info($path_current_testlog) . "\n";
      }

      # Add 20 last executed commands from test case log file
      my $log_file_name = $opt_vardir . "/log/" . $tinfo->{shortname} . ".log";
      if (-e $log_file_name) {
        $tinfo->{comment} .=
          "== $log_file_name == \n" .
          mtr_lastlinesfromfile($log_file_name, 20) . "\n";
      }

      # Mark as timeout
      $tinfo->{'timeout'} = testcase_timeout($tinfo);
      run_on_all($tinfo, 'analyze-timeout');

      # Save timeout information for this testcase to mysqltest.log
      if (-f $path_current_testlog) {
        mtr_appendfile_to_file($path_current_testlog, $path_testlog);
        unlink($path_current_testlog) or
          die "Can't unlink file $path_current_testlog : $!";
      }

      report_failure_and_restart($tinfo);
      return 1;
    }

    mtr_error("Unhandled process $proc exited");
  }
  mtr_error("Should never come here");
}

# Extract server log from after the last occurrence of named test
# Return as an array of lines
sub extract_server_log ($$) {
  my ($error_log, $tname) = @_;

  # Open the servers .err log file and read all lines
  # belonging to current tets into @lines
  my $Ferr = IO::File->new($error_log) or
    mtr_error("Could not open file '$error_log' for reading: $!");

  my @lines;
  my $found_test = 0;    # Set once we've found the log of this test

  while (my $line = <$Ferr>) {
    if ($found_test) {
      # If test wasn't last after all, discard what we found, test again.
      if ($line =~ /^CURRENT_TEST:/) {
        @lines      = ();
        $found_test = $line =~ /^CURRENT_TEST: $tname$/;
      } else {
        push(@lines, $line);
        if (scalar(@lines) > 1000000) {
          $Ferr = undef;
          mtr_warning("Too much log from test, bailing out from extracting");
          return ();
        }
      }
    } else {
      # Search for beginning of test, until found
      $found_test = 1 if ($line =~ /^CURRENT_TEST: $tname$/);
    }
  }
  $Ferr = undef;    # Close error log file

  return @lines;
}

# Get a subset of the lines from a log file
# Returns the first start_lines, and the last end_lines
# of the file, with a <  Snip  > line in the middle if
# there is a gap.
sub ndb_get_log_lines( $$$ ) {
  my ($log_file_name, $start_lines, $end_lines) = @_;

  my $log_file = IO::File->new($log_file_name) or
    mtr_error("Could not open file '$log_file_name' for reading: $!");

  my $total_lines = 0;

  # First pass, get number of lines in the file
  while (my $line = <$log_file>) {
    $total_lines = $total_lines + 1;
  }
  undef $log_file;

  # Now take the lines we want
  my @log_lines;
  my $line_num = 0;

  $log_file = IO::File->new($log_file_name) or
    mtr_error("Could not open file '$log_file_name' for reading: $!");

  while (my $line = <$log_file>) {
    if ($line_num < $start_lines) {
      # Start lines
      push(@log_lines, $line);
    } elsif ($line_num > ($total_lines - $end_lines)) {
      # End lines
      push(@log_lines, $line);
    } elsif ($line_num == $start_lines) {
      # First line in the 'gap'
      my $gap_text = "<  Snip  >";
      push(@log_lines, $gap_text);
    }

    $line_num = $line_num + 1;
  }
  undef $log_file;

  return @log_lines;
}

# Get an ndb crash trace number from a crash trace file name
sub ndb_get_trace_num_from_filename ($) {
  my $trace_file_fq_name = shift;

  # Format is : /basepath/ndb_<id>_trace.log.<trace num>[_t<thread_num>]
  my $trace_file_name      = basename($trace_file_fq_name);
  my @parts                = split(/\./, $trace_file_name);
  my $trace_num_and_thread = $parts[2];

  if ($trace_num_and_thread eq "next") {
    $trace_num_and_thread = 0;
  }

  my @trace_num_and_thread_parts = split(/_/, $trace_num_and_thread);
  my $trace_num = $trace_num_and_thread_parts[0];

  return $trace_num;
}

# Return array of lines containing failed ndbd log info
sub ndb_extract_ndbd_log_info($$) {
  my ($ndbd_log_path, $dump_out_file) = @_;

  my $ndbd_log = "";

  if ($dump_out_file) {
    my $ndbd_log_file_start_lines = 100;
    my $ndbd_log_file_end_lines   = 200;
    my $ndbd_log_file_name        = "$ndbd_log_path/ndbd.log";
    my @log_lines = ndb_get_log_lines($ndbd_log_file_name,
                                      $ndbd_log_file_start_lines,
                                      $ndbd_log_file_end_lines);
    $ndbd_log =
      $ndbd_log . "\nFailed data node output log ($ndbd_log_file_name):\n" .
      "-----------FAILED DATA NODE OUTPUT LOG START--------\n" .
      join("", @log_lines) .
      "-----------FAILED DATA NODE OUTPUT LOG END----------\n";
  }

  # For all ndbds, we look for error and trace files
  #
  my @ndb_error_files = glob("$ndbd_log_path/ndb_*_error.log");
  my $num_error_files = scalar @ndb_error_files;
  if ($num_error_files) {
    # Found an error file, let's go further
    if ($num_error_files > 1) {
      mtr_error(
             "More than one error file found : " . join(" ", @ndb_error_files));
    }

    my $ndbd_error_file_name = $ndb_error_files[0];
    my @log_lines = ndb_get_log_lines($ndbd_error_file_name, 10000, 10000)
      ;    # Get everything from the error file.
    $ndbd_log =
      $ndbd_log . "\nFound data node error log ($ndbd_error_file_name):\n" .
      "\n-----------DATA NODE ERROR LOG START--------\n" .
      join("", @log_lines) . "\n-----------DATA NODE ERROR LOG END----------\n";

    my @ndb_trace_files = glob("$ndbd_log_path/ndb_*_trace*");
    my $num_trace_files = scalar @ndb_trace_files;
    if ($num_trace_files) {
      $ndbd_log = $ndbd_log . "\nFound crash trace files :\n  " .
        join("\n  ", @ndb_trace_files) . "\n\n";

      # Now find most recent set of trace files..
      my $max_trace_num = 0;
      foreach my $trace_file (@ndb_trace_files) {
        my $trace_num = ndb_get_trace_num_from_filename($trace_file);
        if ($trace_num > $max_trace_num) {
          $max_trace_num = $trace_num;
        }
      }

      $ndbd_log =
        $ndbd_log . "\nDumping excerpts from crash number $max_trace_num\n";

      # Now print a chunk of em
      foreach my $trace_file (@ndb_trace_files) {
        if (ndb_get_trace_num_from_filename($trace_file) eq $max_trace_num) {
          my $ndbd_trace_file_start_lines = 2500;
          my $ndbd_trace_file_end_lines   = 0;
          @log_lines =
            ndb_get_log_lines($trace_file, $ndbd_trace_file_start_lines,
                              $ndbd_trace_file_end_lines);

          $ndbd_log =
            $ndbd_log . "\nData node trace file : $trace_file\n" .
            "\n-----------DATA NODE TRACE LOG START--------\n" .
            join("", @log_lines) .
            "\n-----------DATA NODE TRACE LOG END----------\n";
        }
      }
    } else {
      $ndbd_log = $ndbd_log . "\n No trace files! \n";
    }
  }

  return $ndbd_log;
}

# Get log from server identified from its $proc object, from named test
# Return as a single string
sub get_log_from_proc ($$) {
  my ($proc, $name) = @_;
  my $srv_log = "";

  foreach my $mysqld (mysqlds()) {
    if ($mysqld->{proc} eq $proc) {
      my @srv_lines = extract_server_log($mysqld->value('#log-error'), $name);
      $srv_log =
        "\nServer log from this test:\n" .
        "----------SERVER LOG START-----------\n" .
        join("", @srv_lines) . "----------SERVER LOG END-------------\n";
      last;
    }
  }

  # ndbds are started in a directory according to their
  # MTR process 'ids'.  Their ndbd.log files are
  # placed in these directories.
  # Then they allocate a nodeid from mgmd which is really
  # a race, and choose a datadirectory based on the nodeid
  # This can mean that they use a different ndbd.X directory
  # for their ndb_Z_fs files and error + trace log file writing.
  # This could be worked around by setting the ndbd node ids
  # up front, but in the meantime, we will attempt to
  # find error and trace files here.
  foreach my $ndbd (ndbds()) {
    my $ndbd_log_path = $ndbd->value('DataDir');

    my $dump_out_file = ($ndbd->{proc} eq $proc);
    my $ndbd_log_info =
      ndb_extract_ndbd_log_info($ndbd_log_path, $dump_out_file);
    $srv_log = $srv_log . $ndbd_log_info;
  }

  return $srv_log;
}

# Perform a rough examination of the servers error log and write all
# lines that look suspicious into $error_log.warnings file.
sub extract_warning_lines ($$) {
  my ($error_log, $tname) = @_;

  my @lines = extract_server_log($error_log, $tname);

  # Write all suspicious lines to $error_log.warnings file
  my $warning_log = "$error_log.warnings";
  my $Fwarn = IO::File->new($warning_log, "w") or
    die("Could not open file '$warning_log' for writing: $!");
  print $Fwarn "Suspicious lines from $error_log\n";

  my @patterns = (qr/^Warning:|mysqld: Warning|\[Warning\]/,
                  qr/^Error:|\[ERROR\]/,
                  qr/^==\d+==\s+\S/,                           # valgrind errors
                  qr/InnoDB: Warning|InnoDB: Error/,
                  qr/^safe_mutex:|allocated at line/,
                  qr/missing DBUG_RETURN/,
                  qr/Attempting backtrace/,
                  qr/Assertion .* failed/,);

  my $skip_valgrind = 0;
  my $last_pat      = "";
  my $num_rep       = 0;

  foreach my $line (@lines) {
    if ($opt_valgrind_mysqld) {
      # Skip valgrind summary from tests where server has been restarted
      # Should this contain memory leaks, the final report will find it
      # Use a generic pattern for summaries
      $skip_valgrind = 1 if $line =~ /^==\d+== [A-Z ]+ SUMMARY:/;
      $skip_valgrind = 0 unless $line =~ /^==\d+==/;
      next if $skip_valgrind;
    }

    foreach my $pat (@patterns) {
      if ($line =~ /$pat/) {
        # Remove initial timestamp and look for consecutive identical lines
        my $line_pat = $line;
        $line_pat =~ s/^[0-9:\-\+\.TZ ]*//;
        if ($line_pat eq $last_pat) {
          $num_rep++;
        } else {
          # Previous line had been repeated, report that first
          if ($num_rep) {
            print $Fwarn ".... repeated $num_rep times: $last_pat";
            $num_rep = 0;
          }
          $last_pat = $line_pat;
          print $Fwarn $line;
        }
        last;
      }
    }
  }

  # Catch the case of last warning being repeated
  if ($num_rep) {
    print $Fwarn ".... repeated $num_rep times: $last_pat";
  }

  $Fwarn = undef;    # Close file

}

# Run include/check-warnings.test
#
# RETURN VALUE
#   0 OK
#   1 Check failed
sub start_check_warnings ($$) {
  my $tinfo  = shift;
  my $mysqld = shift;

  my $name      = "warnings-" . $mysqld->name();
  my $log_error = $mysqld->value('#log-error');

  # To be communicated to the test
  $ENV{MTR_LOG_ERROR} = $log_error;
  extract_warning_lines($log_error, $tinfo->{name});

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));
  mtr_add_arg($args, "--test-file=%s",  "include/check-warnings.test");
  mtr_add_arg($args, "--logdir=%s/tmp", $opt_vardir);

  my $errfile = "$opt_vardir/tmp/$name.err";
  my $proc = My::SafeProcess->new(name      => $name,
                                  path      => $exe_mysqltest,
                                  error     => $errfile,
                                  output    => $errfile,
                                  args      => \$args,
                                  user_data => $errfile,
                                  verbose   => $opt_verbose,);
  mtr_verbose("Started $proc");
  return $proc;
}

# Loop through our list of processes and check the error log
# for unexepcted errors and warnings.
sub check_warnings ($) {
  my ($tinfo) = @_;
  my $res = 0;

  my $tname = $tinfo->{name};

  # Clear previous warnings
  delete($tinfo->{warnings});

  # Start the mysqltest processes in parallel to save time also makes
  # it possible to wait for any process to exit during the check.
  my %started;
  foreach my $mysqld (mysqlds()) {
    if (defined $mysqld->{'proc'}) {
      my $proc = start_check_warnings($tinfo, $mysqld);
      $started{ $proc->pid() } = $proc;
    }
  }

  # Return immediately if no check proceess was started
  return 0 unless (keys %started);

  my $timeout = start_timer(check_timeout($tinfo));

  while (1) {
    my $result = 0;
    my $proc   = My::SafeProcess->wait_any_timeout($timeout);
    mtr_report("Got $proc");

    # Delete the 'check-warnings.log' file generated after
    #  check-warnings.test run is completed
    my $check_warnings_log_file = "$opt_vardir/tmp/check-warnings.log";
    if (-e $check_warnings_log_file) {
      unlink($check_warnings_log_file);
    }

    if (delete $started{ $proc->pid() }) {
      # One check warning process returned
      my $res      = $proc->exit_status();
      my $err_file = $proc->user_data();

      if ($res == 0 or $res == 62) {
        if ($res == 0) {
          # Check completed with problem
          my $report = mtr_grab_file($err_file);

          # In rare cases on Windows, exit code 62 is lost, so check output
          if (IS_WINDOWS and
              $report =~ /^The test .* is not supported by this installation/) {
            # Extra sanity check
            if ($report =~ /^reason: OK$/m) {
              $res = 62;
              mtr_print("Seems to have lost exit code 62, assume no warn\n");
              goto LOST62;
            }
          }

          # Log to var/log/warnings file
          mtr_tofile("$opt_vardir/log/warnings", $tname . "\n" . $report);

          $tinfo->{'warnings'} .= $report;
          $result = 1;
        }
      LOST62:
        if ($res == 62) {
          # Test case was ok and called "skip", remove the .err file
          # the check generated.
          unlink($err_file);
        }

        if (keys(%started) == 0) {
          # All checks completed
          mark_time_used('ch-warn');
          return $result;
        }

        # Wait for next process to exit
        next if not $result;
      } else {
        my $report = mtr_grab_file($err_file);
        $tinfo->{comment} .=
          "Could not execute 'check-warnings' for " .
          "testcase '$tname' (res: $res):\n";
        $tinfo->{comment} .= $report;
        $result = 2;
      }
    } elsif ($proc->{timeout}) {
      $tinfo->{comment} .= "Timeout for 'check warnings' expired after " .
        check_timeout($tinfo) . " seconds";
      $result = 4;
    } else {
      # Unknown process returned, most likley a crash, abort everything
      $tinfo->{comment} =
        "The server $proc crashed while running 'check warnings'" .
        get_log_from_proc($proc, $tinfo->{name});
      $result = 3;
    }

    # Kill any check processes still running
    map($_->kill(), values(%started));

    mark_time_used('ch-warn');
    return $result;
  }

  mtr_error("INTERNAL_ERROR: check_warnings");
}

# Loop through our list of processes and look for and entry with the
# provided pid, if found check for the file indicating expected crash
# and restart it.
sub check_expected_crash_and_restart {
  my ($proc) = @_;

  foreach my $mysqld (mysqlds()) {
    next unless ($mysqld->{proc} and $mysqld->{proc} eq $proc);

    # Check if crash expected by looking at the .expect file in var/tmp
    my $expect_file = "$opt_vardir/tmp/" . $mysqld->name() . ".expect";
    if (-f $expect_file) {
      mtr_verbose("Crash was expected, file '$expect_file' exists");
      for (my $waits = 0 ; $waits < 50 ; mtr_milli_sleep(100), $waits++) {
        # Race condition seen on Windows: try again until file not empty
        next if -z $expect_file;
        # If last line in expect file starts with "wait" sleep a little
        # and try again, thus allowing the test script to control when
        # the server should start up again. Keep trying for up to 5 seconds
        # at a time.
        my $last_line = mtr_lastlinesfromfile($expect_file, 1);
        if ($last_line =~ /^wait/) {
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
          my @rest_opt = split(' ', $1);
          $mysqld->{'restart_opts'} = \@rest_opt;
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
  my ($dir) = @_;
  mtr_verbose("clean_dir: $dir");

  finddepth(
    { no_chdir => 1,
      wanted   => sub {
        if (-d $_) {
          # A dir
          if ($_ eq $dir) {
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

  if (started(all_servers()) != 0) {
    mtr_error("Trying to clean datadir before all servers stopped");
  }

  foreach my $cluster (clusters()) {
    my $cluster_dir = "$opt_vardir/" . $cluster->{name};
    mtr_verbose(" - removing '$cluster_dir'");
    rmtree($cluster_dir);
  }

  foreach my $mysqld (mysqlds()) {
    my $mysqld_dir = dirname($mysqld->value('datadir'));
    if (-d $mysqld_dir) {
      mtr_verbose(" - removing '$mysqld_dir'");
      rmtree($mysqld_dir);
    }
  }

  # Remove all files in tmp and var/tmp
  clean_dir("$opt_vardir/tmp");
  if ($opt_tmpdir ne "$opt_vardir/tmp") {
    clean_dir($opt_tmpdir);
  }
}

# Save datadir before it's removed
sub save_datadir_after_failure($$) {
  my ($dir, $savedir) = @_;

  mtr_report(" - saving '$dir'");
  my $dir_name = basename($dir);
  rename("$dir", "$savedir/$dir_name");
}

sub remove_ndbfs_from_ndbd_datadir {
  my ($ndbd_datadir) = @_;
  # Remove the ndb_*_fs directory from ndbd.X/ dir
  foreach my $ndbfs_dir (glob("$ndbd_datadir/ndb_*_fs")) {
    next unless -d $ndbfs_dir;    # Skip if not a directory
    rmtree($ndbfs_dir);
  }
}

sub after_failure ($) {
  my ($tinfo) = @_;

  mtr_report("Saving datadirs...");

  my $save_dir = "$opt_vardir/log/";
  $save_dir .= $tinfo->{name};

  # Add combination name if any
  $save_dir .= "-$tinfo->{combination}" if defined $tinfo->{combination};

  # Save savedir  path for server
  $tinfo->{savedir} = $save_dir;

  mkpath($save_dir) if !-d $save_dir;

  # Save the used my.cnf file
  copy($path_config_file, $save_dir);

  # Copy the tmp dir
  copytree("$opt_vardir/tmp/", "$save_dir/tmp/");

  if (clusters()) {
    foreach my $cluster (clusters()) {
      my $cluster_dir = "$opt_vardir/" . $cluster->{name};

      # Remove the fileystem of each ndbd
      foreach my $ndbd (in_cluster($cluster, ndbds())) {
        my $ndbd_datadir = $ndbd->value("DataDir");
        remove_ndbfs_from_ndbd_datadir($ndbd_datadir);
      }

      save_datadir_after_failure($cluster_dir, $save_dir);
    }
  } else {
    foreach my $mysqld (mysqlds()) {
      my $data_dir = $mysqld->value('datadir');
      save_datadir_after_failure(dirname($data_dir), $save_dir);
    }
  }
}

sub report_failure_and_restart ($) {
  my $tinfo = shift;

  if ($opt_valgrind_mysqld && ($tinfo->{'warnings'} || $tinfo->{'timeout'})) {
    # In these cases we may want valgrind report from normal termination
    $tinfo->{'dont_kill_server'} = 1;
  }

  # Shotdown properly if not to be killed (for valgrind)
  stop_all_servers($tinfo->{'dont_kill_server'} ? $opt_shutdown_timeout : 0);

  $tinfo->{'result'} = 'MTR_RES_FAILED';

  my $test_failures = $tinfo->{'failures'} || 0;
  $tinfo->{'failures'} = $test_failures + 1;

  if ($tinfo->{comment}) {
    # The test failure has been detected by mysql-test-run.pl
    # when starting the servers or due to other error, the reason for
    # failing the test is saved in "comment"
    ;
  }

  if (!defined $tinfo->{logfile} and !$tinfo->{'no_result_file'}) {
    my $logfile = $path_current_testlog;
    if (defined $logfile) {
      if (-f $logfile) {
        # Test failure was detected by test tool and its report
        # about what failed has been saved to file. Save the report
        # in tinfo
        $tinfo->{logfile} = mtr_fromfile($logfile);
        # If no newlines in the test log:
        # (it will contain the CURRENT_TEST written by mtr, so is not empty)
        if ($tinfo->{logfile} !~ /\n/) {
          # Show how far it got before suddenly failing
          $tinfo->{comment} .= "mysqltest failed but provided no output\n";
          my $log_file_name =
            $opt_vardir . "/log/" . $tinfo->{shortname} . ".log";
          if (-e $log_file_name) {
            $tinfo->{comment} .=
              "The result from queries just before the failure was:" .
              "\n< snip >\n" . mtr_lastlinesfromfile($log_file_name, 20) . "\n";
          }
        }
      } else {
        # The test tool report didn't exist, display an error message
        $tinfo->{logfile} = "Could not open test tool report '$logfile'";
      }
    }
  }

  after_failure($tinfo);
  mtr_report_test($tinfo);
}

sub run_sh_script {
  my ($script) = @_;
  return 0 unless defined $script;
  mtr_verbose("Running '$script'");
  my $ret = system("/bin/sh $script") >> 8;
  return $ret;
}

sub mysqld_stop {
  my $mysqld = shift or die "usage: mysqld_stop(<mysqld>)";

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--character-sets-dir=%s",
              $mysqld->value('character-sets-dir'));
  mtr_add_arg($args, "--user=%s", $opt_user);
  mtr_add_arg($args, "--password=");
  mtr_add_arg($args, "--port=%d", $mysqld->value('port'));
  mtr_add_arg($args, "--host=%s", $mysqld->value('#host'));
  mtr_add_arg($args, "--connect_timeout=20");
  mtr_add_arg($args, "--protocol=tcp");
  mtr_add_arg($args, "shutdown");

  My::SafeProcess->run(name  => "mysqladmin shutdown " . $mysqld->name(),
                       path  => $exe_mysqladmin,
                       args  => \$args,
                       error => "/dev/null",);
}

# This subroutine is added to handle option file options which always
# have to be passed before any other variables or command line options.
sub arrange_option_files_options {
  my ($args, $mysqld, $extra_opts, @options) = @_;

  my @optionfile_options;
  foreach my $arg (@$extra_opts) {
    if (grep { $arg =~ $_ } @options) {
      push(@optionfile_options, $arg);
    }
  }

  my $opt_no_defaults    = grep(/^--no-defaults/,         @optionfile_options);
  my $opt_defaults_extra = grep(/^--defaults-extra-file/, @optionfile_options);
  my $opt_defaults       = grep(/^--defaults-file/,       @optionfile_options);

  if (!$opt_defaults_extra && !$opt_defaults && !$opt_no_defaults) {
    mtr_add_arg($args, "--defaults-file=%s", $path_config_file);
  }

  push(@$args, @optionfile_options);

  # no-defaults has to be the first option provided to mysqld.
  if ($opt_no_defaults) {
    @$args = grep { !/^--defaults-group-suffix/ } @$args;
  }
}

sub mysqld_arguments ($$$) {
  my $args       = shift;
  my $mysqld     = shift;
  my $extra_opts = shift;

  my @options = ("--no-defaults",   "--defaults-extra-file",
                 "--defaults-file", "--login-path",
                 "--print-defaults");
  arrange_option_files_options($args, $mysqld, $extra_opts, @options);

  # When mysqld is run by a root user(euid is 0), it will fail
  # to start unless we specify what user to run as, see BUG#30630
  my $euid = $>;
  if (!IS_WINDOWS and $euid == 0 and (grep(/^--user/, @$extra_opts)) == 0) {
    mtr_add_arg($args, "--user=root");
  }

  if ($opt_valgrind_mysqld) {
    if ($mysql_version_id < 50100) {
      mtr_add_arg($args, "--skip-bdb");
    }
  }

  if ($mysql_version_id >= 50106 && !$opt_user_args) {
    # Turn on logging to file
    mtr_add_arg($args, "--log-output=file");
  }

  # Indicate to mysqld it will be debugged in debugger
  if ($glob_debugger) {
    mtr_add_arg($args, "--gdb");
  }

  # Enable the debug sync facility, set default wait timeout.
  # Facility stays disabled if timeout value is zero.
  mtr_add_arg($args, "--loose-debug-sync-timeout=%s", $opt_debug_sync_timeout)
    unless $opt_user_args;

  # Options specified in .opt files should be added last so they can
  # override defaults above.

  my $found_skip_core  = 0;
  my $found_no_console = 0;
  my $found_log_error  = 0;

  # Check if the option 'log-error' is found in the .cnf file
  # In the group defined for the server
  $found_log_error = 1
    if defined $mysqld->option("log-error") or
    defined $mysqld->option("log_error");

  # In the [mysqld] section
  $found_log_error = 1
    if !$found_log_error   and
    defined mysqld_group() and
    (defined mysqld_group()->option("log-error") or
     defined mysqld_group()->option("log_error"));

  foreach my $arg (@$extra_opts) {
    # Skip option file options because they are handled above
    next if (grep { $arg =~ $_ } @options);

    if ($arg =~ /--init[-_]file=(.*)/) {
      # If the path given to the init file is relative
      # to an out of source build, the file should
      # be looked for in the MTR_BINDIR.
      $arg = "--init-file=" . get_bld_path($1);
    } elsif ($arg =~ /--log[-_]error=/ or $arg =~ /--log[-_]error$/) {
      $found_log_error = 1;
    } elsif ($arg eq "--skip-core-file") {
      # Allow --skip-core-file to be set in <testname>-[master|slave].opt file
      $found_skip_core = 1;
      next;
    } elsif ($arg eq "--no-console") {
      $found_no_console = 1;
      next;
    } elsif ($arg =~ /--loose[-_]skip[-_]log[-_]bin/ and
             $mysqld->option("log-slave-updates")) {
      # Dont add --skip-log-bin when mysqld has --log-slave-updates in config
      next;
    } elsif ($arg eq "") {
      # We can get an empty argument when  we set environment variables to ""
      # (e.g plugin not found). Just skip it.
      next;
    } elsif ($arg eq "--daemonize") {
      $mysqld->{'daemonize'} = 1;
    }

    mtr_add_arg($args, "%s", $arg);
  }

  $opt_skip_core = $found_skip_core;
  if (IS_WINDOWS && !$found_no_console && !$found_log_error) {
    # Trick the server to send output to stderr, with --console
    mtr_add_arg($args, "--console");
  }

  if (!$found_skip_core && !$opt_user_args) {
    mtr_add_arg($args, "%s", "--core-file");
  }

  return $args;
}

sub mysqld_start ($$) {
  my $mysqld     = shift;
  my $extra_opts = shift;

  mtr_verbose(My::Options::toStr("mysqld_start", @$extra_opts));

  my $exe               = find_mysqld($mysqld->value('#mtr_basedir'));
  my $wait_for_pid_file = 1;

  my $args;
  mtr_init_args(\$args);

  # Implementation for strace-server
  if ($opt_strace_server) {
    strace_server_arguments($args, \$exe, $mysqld->name());
  }

  foreach my $arg (@$extra_opts) {
    $daemonize_mysqld = 1 if ($arg eq "--daemonize");
  }

  if ($opt_valgrind_mysqld) {
    valgrind_arguments($args, \$exe, $mysqld->name());
  }

  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));

  # Add any additional options from an in-test restart
  my @all_opts = @$extra_opts;
  if (exists $mysqld->{'restart_opts'}) {
    push(@all_opts, @{ $mysqld->{'restart_opts'} });
    mtr_verbose(
       My::Options::toStr("mysqld_start restart", @{ $mysqld->{'restart_opts'} }
       ));
  }
  mysqld_arguments($args, $mysqld, \@all_opts);

  if ($opt_debug) {
    mtr_add_arg($args, "--debug=$debug_d:t:i:A,%s/log/%s.trace",
                $path_vardir_trace, $mysqld->name());
  }

  if (IS_WINDOWS) {
    mtr_add_arg($args, "--enable-named-pipe");
  }

  if ($opt_gdb || $opt_manual_gdb) {
    gdb_arguments(\$args, \$exe, $mysqld->name());
  } elsif ($opt_lldb || $opt_manual_lldb) {
    lldb_arguments(\$args, \$exe, $mysqld->name());
  } elsif ($opt_ddd || $opt_manual_ddd) {
    ddd_arguments(\$args, \$exe, $mysqld->name());
  } elsif ($opt_dbx || $opt_manual_dbx) {
    dbx_arguments(\$args, \$exe, $mysqld->name());
  } elsif ($opt_debugger) {
    debugger_arguments(\$args, \$exe, $mysqld->name());
  } elsif ($opt_manual_debug) {
    print "\nStart " . $mysqld->name() . " in your debugger\n" .
      "dir: $glob_mysql_test_dir\n" . "exe: $exe\n" . "args:  " .
      join(" ", @$args) . "\n\n" . "Waiting ....\n";

    # Indicate the exe should not be started
    $exe = undef;
  } else {
    # Default to not wait until pid file has been created
    $wait_for_pid_file = 0;
  }

  # Remove the old pidfile if any
  unlink($mysqld->value('pid-file'));

  my $output = $mysqld->value('#log-error');

  # Remember this log file for valgrind error report search
  $mysqld_logs{$output} = 1 if $opt_valgrind or $opt_sanitize;

  # Remember data dir for gmon.out files if using gprof
  $gprof_dirs{ $mysqld->value('datadir') } = 1 if $opt_gprof;

  if (defined $exe) {
    $mysqld->{'proc'} =
      My::SafeProcess->new(name        => $mysqld->name(),
                           path        => $exe,
                           args        => \$args,
                           output      => $output,
                           error       => $output,
                           append      => 1,
                           verbose     => $opt_verbose,
                           nocore      => $opt_skip_core,
                           host        => undef,
                           shutdown    => sub { mysqld_stop($mysqld) },
                           envs        => \@opt_mysqld_envs,
                           pid_file    => $mysqld->value('pid-file'),
                           daemon_mode => $mysqld->{'daemonize'});

    mtr_verbose("Started $mysqld->{proc}");
  }

  if ($wait_for_pid_file &&
      !sleep_until_pid_file_created($mysqld->value('pid-file'),
                                    $opt_start_timeout,
                                    $mysqld->{'proc'})
  ) {
    my $mname = $mysqld->name();
    mtr_error("Failed to start mysqld $mname with command $exe");
  }

  # Remember options used when starting
  $mysqld->{'started_opts'} = $extra_opts;

  return;
}

sub stop_all_servers () {
  my $shutdown_timeout = $_[0] or 0;

  mtr_verbose("Stopping all servers...");

  # Kill all started servers
  My::SafeProcess::shutdown($shutdown_timeout, started(all_servers()));

  # Remove pidfiles
  foreach my $server (all_servers()) {
    my $pid_file = $server->if_exist('pid-file');
    unlink($pid_file) if defined $pid_file;
  }

  # Mark servers as stopped
  map($_->{proc} = undef, all_servers());
}

sub is_slave {
  my ($server) = @_;
  # There isn't really anything in a configuration which tells if a
  # mysqld is master or slave. Best guess is to treat all which haven't
  # got '#!use-slave-opt' as masters. At least be consistent.
  return $server->option('#!use-slave-opt');
}

# Find out if server should be restarted for this test
sub server_need_restart {
  my ($tinfo, $server, $master_restarted) = @_;

  if (using_extern()) {
    mtr_verbose_restart($server, "no restart for --extern server");
    return 0;
  }

  if ($tinfo->{'force_restart'}) {
    mtr_verbose_restart($server, "forced in .opt file");
    return 1;
  }

  if ($opt_force_restart) {
    mtr_verbose_restart($server, "forced restart turned on");
    return 1;
  }

  if ($tinfo->{template_path} ne $current_config_name) {
    mtr_verbose_restart($server, "using different config file");
    return 1;
  }

  if ($tinfo->{'master_sh'} || $tinfo->{'slave_sh'}) {
    mtr_verbose_restart($server, "sh script to run");
    return 1;
  }

  if (!started($server)) {
    mtr_verbose_restart($server, "not started");
    return 1;
  }

  my $started_tinfo = $server->{'started_tinfo'};
  if (defined $started_tinfo) {
    # Check if timezone of  test that server was started
    # with differs from timezone of next test
    if (timezone($started_tinfo) ne timezone($tinfo)) {
      mtr_verbose_restart($server, "different timezone");
      return 1;
    }
  }

  my $is_mysqld = grep ($server eq $_, mysqlds());
  if ($is_mysqld) {
    # Check that running process was started with same options
    # as the current test requires
    my $extra_opts = get_extra_opts($server, $tinfo);
    my $started_opts = $server->{'started_opts'};

    # Also, always restart if server had been restarted with additional
    # options within test.
    if (!My::Options::same($started_opts, $extra_opts) ||
        exists $server->{'restart_opts'}) {
      my $use_dynamic_option_switch = 0;
      if (!$use_dynamic_option_switch) {
        mtr_verbose_restart($server,
                            "running with different options '" .
                              join(" ", @{$extra_opts}) . "' != '" .
                              join(" ", @{$started_opts}) . "'");
        return 1;
      }

      mtr_verbose(My::Options::toStr("started_opts", @$started_opts));
      mtr_verbose(My::Options::toStr("extra_opts",   @$extra_opts));

      # Get diff and check if dynamic switch is possible
      my @diff_opts = My::Options::diff($started_opts, $extra_opts);
      mtr_verbose(My::Options::toStr("diff_opts", @diff_opts));

      my $query = My::Options::toSQL(@diff_opts);
      mtr_verbose("Attempting dynamic switch '$query'");
      if (run_query($tinfo, $server, $query)) {
        mtr_verbose("Restart: running with different options '" .
                    join(" ", @{$extra_opts}) . "' != '" .
                    join(" ", @{$started_opts}) . "'");
        return 1;
      }

      # Remember the dynamically set options
      $server->{'started_opts'} = $extra_opts;
    }

    if (is_slave($server) && $master_restarted) {
      # At least one master restarted and this is a slave, restart
      mtr_verbose_restart($server, " master restarted");
      return 1;
    }
  }

  # Default, no restart
  return 0;
}

sub servers_need_restart($) {
  my ($tinfo) = @_;

  my @restart_servers;

  # Build list of master and slave mysqlds to be able to restart
  # all slaves whenever a master restarts.
  my @masters;
  my @slaves;

  foreach my $server (mysqlds()) {
    if (is_slave($server)) {
      push(@slaves, $server);
    } else {
      push(@masters, $server);
    }
  }

  # Check masters
  my $master_restarted = 0;
  foreach my $master (@masters) {
    if (server_need_restart($tinfo, $master, $master_restarted)) {
      $master_restarted = 1;
      push(@restart_servers, $master);
    }
  }

  # Check slaves
  foreach my $slave (@slaves) {
    if (server_need_restart($tinfo, $slave, $master_restarted)) {
      push(@restart_servers, $slave);
    }
  }

  # Check if any remaining servers need restart
  foreach my $server (ndb_mgmds(), ndbds(), memcacheds()) {
    if (server_need_restart($tinfo, $server, $master_restarted)) {
      push(@restart_servers, $server);
    }
  }

  return @restart_servers;
}

# Filter a list of servers and return only those that are part
# of the specified cluster
sub in_cluster {
  my ($cluster) = shift;
  # Return only processes for a specific cluster
  return grep { $_->suffix() eq $cluster->suffix() } @_;
}

# Filter a list of servers and return the SafeProcess
# for only those that are started or stopped
sub started { return grep(defined $_,  map($_->{proc}, @_)); }
sub stopped { return grep(!defined $_, map($_->{proc}, @_)); }

sub envsubst {
  my $string = shift;

  # Check for the ? symbol in the var name and remove it.
  if ($string =~ s/^\?//) {
    if (!defined $ENV{$string}) {
      return "";
    }
  } else {
    if (!defined $ENV{$string}) {
      mtr_error(".opt file references '$string' which is not set");
    }
  }

  return $ENV{$string};
}

sub get_extra_opts {
  # No extra options if --user-args
  return \@opt_extra_mysqld_opt if $opt_user_args;

  my ($mysqld, $tinfo) = @_;

  my $opts =
    $mysqld->option("#!use-slave-opt") ? $tinfo->{slave_opt} :
    $tinfo->{master_opt};

  # Expand environment variables
  foreach my $opt (@$opts) {
    $opt =~ s/\$\{(\??\w+)\}/envsubst($1)/ge;
    $opt =~ s/\$(\??\w+)/envsubst($1)/ge;
  }
  return $opts;
}

# Collect client options from client.opt file
sub get_client_options($$) {
  my ($args, $tinfo) = @_;

  foreach my $opt (@{ $tinfo->{client_opt} }) {
    # Expand environment variables
    $opt =~ s/\$\{(\??\w+)\}/envsubst($1)/ge;
    $opt =~ s/\$(\??\w+)/envsubst($1)/ge;
    mtr_add_arg($args, $opt);
  }
}

sub stop_servers($$) {
  my ($tinfo, @servers) = @_;

  # Remember if we restarted for this test case (count restarts)
  $tinfo->{'restarted'} = 1;

  if (join('|', @servers) eq join('|', all_servers())) {
    # All servers are going down, use some kind of order to
    # avoid too many warnings in the log files

    mtr_report("Restarting all servers");

    # mysqld processes
    My::SafeProcess::shutdown($opt_shutdown_timeout, started(mysqlds()));

    # cluster processes
    My::SafeProcess::shutdown($opt_shutdown_timeout,
                              started(ndbds(), ndb_mgmds(), memcacheds()));
  } else {
    mtr_report("Restarting ", started(@servers));

    # Stop only some servers
    My::SafeProcess::shutdown($opt_shutdown_timeout, started(@servers));
  }

  foreach my $server (@servers) {
    # Mark server as stopped
    $server->{proc} = undef;

    # Forget history
    delete $server->{'started_tinfo'};
    delete $server->{'started_opts'};
    delete $server->{'started_cnf'};
    delete $server->{'restart_opts'};
  }
}

# Start servers not already started
#
# RETURN
#   0 OK
#   1 Start failed
sub start_servers($) {
  my ($tinfo) = @_;

  # Make sure the safe_process also exits from now on.  Could not be
  # done before, as we don't want this for the bootstrap.
  if ($opt_start_exit) {
    My::SafeProcess->start_exit();
  }

  # Start clusters
  foreach my $cluster (clusters()) {
    ndbcluster_start($cluster);
  }

  my $server_id = 0;

  # Start mysqlds
  foreach my $mysqld (mysqlds()) {
    # Group Replication requires a local port to be open on each server
    # in order to receive messages from the group, Store the reserved
    # port in an environment variable SERVER_GR_PORT_X(where X is the
    # server number).
    $server_id++;
    my $xcom_server = "SERVER_GR_PORT_" . $server_id;

    if (!$group_replication) {
      # Assigning error value '-1' to SERVER_GR_PORT_X environment
      # variable, since the number of ports reserved per thread is not
      # enough for allocating extra Group replication ports.
      $ENV{$xcom_server} = -1;
    } else {
      my $xcom_port = $baseport + 9 + $server_id;
      $ENV{$xcom_server} = $xcom_port;
    }

    if ($mysqld->{proc}) {
      # Already started, write start of testcase to log file
      mark_log($mysqld->value('#log-error'), $tinfo);
      next;
    }

    my $datadir = $mysqld->value('datadir');
    if ($opt_start_dirty) {
      # Don't delete anything if starting dirty
      ;
    } else {
      my @options = ('log-bin', 'relay-log');
      foreach my $option_name (@options) {
        next unless $mysqld->option($option_name);

        my $file_name = $mysqld->value($option_name);
        next unless defined $file_name and -e $file_name;

        mtr_verbose(" -removing '$file_name'");
        unlink($file_name) or die("unable to remove file '$file_name'");
      }

      if (-d $datadir) {
        mtr_verbose(" - removing '$datadir'");
        rmtree($datadir);
      }
    }

    my $mysqld_basedir = $mysqld->value('#mtr_basedir');
    if ($basedir eq $mysqld_basedir) {
      # If dirty, keep possibly grown system db
      if (!$opt_start_dirty) {
        # Copy datadir from installed system db
        my $path = ($opt_parallel == 1) ? "$opt_vardir" : "$opt_vardir/..";
        my $install_db = "$path/data/";
        copytree($install_db, $datadir) if -d $install_db;
        mtr_error("Failed to copy system db to '$datadir'")
          unless -d $datadir;

        # Restore the value of bootstrap command for the next run.
        if ($initial_bootstrap_cmd ne $ENV{'MYSQLD_BOOTSTRAP_CMD'}) {
          $ENV{'MYSQLD_BOOTSTRAP_CMD'} = $initial_bootstrap_cmd;
        }
      }
    } else {
      mysql_install_db($mysqld);    # For versional testing
      mtr_error("Failed to install system db to '$datadir'")
        unless -d $datadir;
    }

    # Reinitialize the data directory if there are bootstrap options
    # in the opt file.
    my $bootstrap_opts = (is_slave($mysqld) ? $tinfo->{bootstrap_slave_opt} :
                            $tinfo->{bootstrap_master_opt});

    if ($bootstrap_opts) {
      clean_dir($datadir);
      mysql_install_db($mysqld, $datadir, $bootstrap_opts);

      # Remove the bootstrap.sql file so that a duplicate set of
      # SQL statements do not get written to the same file.
      unlink("$opt_vardir/tmp/bootstrap.sql")
        if (-f "$opt_vardir/tmp/bootstrap.sql");
    }

    # Create the servers tmpdir
    my $tmpdir = $mysqld->value('tmpdir');
    mkpath($tmpdir) unless -d $tmpdir;

    # Write start of testcase to log file
    mark_log($mysqld->value('#log-error'), $tinfo);

    # Run <tname>-master.sh
    if ($mysqld->option('#!run-master-sh') and
        run_sh_script($tinfo->{master_sh})) {
      $tinfo->{'comment'} = "Failed to execute '$tinfo->{master_sh}'";
      return 1;
    }

    # Run <tname>-slave.sh
    if ($mysqld->option('#!run-slave-sh') and
        run_sh_script($tinfo->{slave_sh})) {
      $tinfo->{'comment'} = "Failed to execute '$tinfo->{slave_sh}'";
      return 1;
    }

    my $extra_opts = get_extra_opts($mysqld, $tinfo);
    mysqld_start($mysqld, $extra_opts);

    # Save this test case information, so next can examine it
    $mysqld->{'started_tinfo'} = $tinfo;
  }

  # Wait for clusters to start
  foreach my $cluster (clusters()) {
    if (ndbcluster_wait_started($cluster, "")) {
      # failed to start
      $tinfo->{'comment'} =
        "Start of '" . $cluster->name() . "' cluster failed";

      # Dump cluster log files to log file to help analyze the
      # cause of the failed start
      ndbcluster_dump($cluster);

      return 1;
    }
  }

  # Wait for mysqlds to start
  foreach my $mysqld (mysqlds()) {
    next if !started($mysqld);

    if (!sleep_until_pid_file_created($mysqld->value('pid-file'),
                                      $opt_start_timeout,
                                      $mysqld->{'proc'})
    ) {
      $tinfo->{comment} = "Failed to start " . $mysqld->name();
      my $logfile = $mysqld->value('#log-error');
      if (defined $logfile and -f $logfile) {
        my @srv_lines = extract_server_log($logfile, $tinfo->{name});
        $tinfo->{logfile} = "Server log is:\n" . join("", @srv_lines);
      } else {
        $tinfo->{logfile} = "Could not open server logfile: '$logfile'";
      }
      return 1;
    }
  }

  # Start memcached(s) for each cluster
  foreach my $cluster (clusters()) {
    next if !in_cluster($cluster, memcacheds());

    # Load the memcache metadata into this cluster
    memcached_load_metadata($cluster);

    # Start memcached(s)
    foreach my $memcached (in_cluster($cluster, memcacheds())) {
      next if started($memcached);
      memcached_start($cluster, $memcached);
    }
  }

  return 0;
}

# Run include/check-testcase.test. Before a testcase, run in record
# mode and save result file to var/tmp. After testcase, run and compare
# with the recorded file, they should be equal.
#
# RETURN VALUE
#   The newly started process
sub start_check_testcase ($$$) {
  my $tinfo  = shift;
  my $mode   = shift;
  my $mysqld = shift;

  my $name = "check-" . $mysqld->name();
  # Replace dots in name with underscore to avoid that mysqltest
  # misinterpret's what the filename extension is :(
  $name =~ s/\./_/g;

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--defaults-file=%s",         $path_config_file);
  mtr_add_arg($args, "--defaults-group-suffix=%s", $mysqld->after('mysqld'));
  mtr_add_arg($args, "--result-file=%s", "$opt_vardir/tmp/$name.result");
  mtr_add_arg($args, "--test-file=%s",   "include/check-testcase.test");
  mtr_add_arg($args, "--verbose");
  mtr_add_arg($args, "--logdir=%s/tmp",  $opt_vardir);

  if (IS_WINDOWS) {
    mtr_add_arg($args, "--protocol=pipe");
  }

  if ($mode eq "before") {
    mtr_add_arg($args, "--record");
  }

  my $errfile = "$opt_vardir/tmp/$name.err";
  my $proc = My::SafeProcess->new(name      => $name,
                                  path      => $exe_mysqltest,
                                  error     => $errfile,
                                  output    => $errfile,
                                  args      => \$args,
                                  user_data => $errfile,
                                  verbose   => $opt_verbose,);

  mtr_report("Started $proc");
  return $proc;
}

sub run_mysqltest ($) {
  my $proc = start_mysqltest(@_);
  $proc->wait();
}

sub start_mysqltest ($) {
  my ($tinfo) = @_;
  my $exe = $exe_mysqltest;
  my $args;

  mark_time_used('admin');

  mtr_init_args(\$args);

  if ($opt_strace_client) {
    $exe = "strace";
    mtr_add_arg($args, "-o");
    mtr_add_arg($args, "%s/log/mysqltest.strace", $opt_vardir);
    mtr_add_arg($args, "-f");
    mtr_add_arg($args, "$exe_mysqltest");
  }

  mtr_add_arg($args, "--defaults-file=%s",      $path_config_file);
  mtr_add_arg($args, "--silent");
  mtr_add_arg($args, "--tmpdir=%s",             $opt_tmpdir);
  mtr_add_arg($args, "--character-sets-dir=%s", $path_charsetsdir);
  mtr_add_arg($args, "--logdir=%s/log",         $opt_vardir);
  mtr_add_arg($args, "--database=test");

  if ($auth_plugin) {
    mtr_add_arg($args, "--plugin_dir=%s", dirname($auth_plugin));
  }

  # Log line number and time  for each line in .test file
  if ($opt_mark_progress) {
    mtr_add_arg($args, "--mark-progress");
  }

  if ($opt_ps_protocol) {
    mtr_add_arg($args, "--ps-protocol");
  }

  if ($opt_no_skip) {
    mtr_add_arg($args, "--no-skip");
    mtr_add_arg($args, "--no-skip-exclude-list=$excluded_string");
  }

  if ($opt_sp_protocol) {
    mtr_add_arg($args, "--sp-protocol");
  }

  if ($opt_explain_protocol) {
    mtr_add_arg($args, "--explain-protocol");
  }

  if ($opt_json_explain_protocol) {
    mtr_add_arg($args, "--json-explain-protocol");
  }

  if ($opt_view_protocol) {
    mtr_add_arg($args, "--view-protocol");
  }

  if ($opt_trace_protocol) {
    mtr_add_arg($args, "--opt-trace-protocol");
  }

  if ($opt_cursor_protocol) {
    mtr_add_arg($args, "--cursor-protocol");
  }

  mtr_add_arg($args, "--timer-file=%s/log/timer", $opt_vardir);

  if ($opt_compress) {
    mtr_add_arg($args, "--compress");
  }

  if ($opt_sleep) {
    mtr_add_arg($args, "--sleep=%d", $opt_sleep);
  }

  if ($opt_ssl) {
    # Turn on SSL for _all_ test cases if option --ssl was used
    mtr_add_arg($args, "--ssl-mode=REQUIRED");
  }

  if ($opt_max_connections) {
    mtr_add_arg($args, "--max-connections=%d", $opt_max_connections);
  }

  if ($opt_colored_diff) {
    mtr_add_arg($args, "--colored-diff", $opt_colored_diff);
  }

  foreach my $arg (@opt_extra_mysqltest_opt) {
    mtr_add_arg($args, $arg);
  }

  # Check for any client options
  get_client_options($args, $tinfo) if $tinfo->{client_opt};

  # Export MYSQL_TEST variable containing <path>/mysqltest <args>
  $ENV{'MYSQL_TEST'} = mtr_args2str($exe_mysqltest, @$args);

  # Add arguments that should not go into the MYSQL_TEST env var
  if ($opt_valgrind_mysqltest) {
    # Prefix the Valgrind options to the argument list. We do this
    # here, since we do not want to Valgrind the nested invocations
    # of mysqltest; that would mess up the stderr output causing
    # test failure.
    my @args_saved = @$args;
    mtr_init_args(\$args);
    valgrind_arguments($args, \$exe);
    mtr_add_arg($args, "%s", $_) for @args_saved;
  }

  mtr_add_arg($args, "--test-file=%s", $tinfo->{'path'});

  # Number of lines of resut to include in failure report
  mtr_add_arg($args, "--tail-lines=20");

  if (defined $tinfo->{'result_file'}) {
    mtr_add_arg($args, "--result-file=%s", $tinfo->{'result_file'});
  }

  client_debug_arg($args, "mysqltest");

  if ($opt_record) {
    $ENV{'MTR_RECORD'} = 1;
    mtr_add_arg($args, "--record");

    # When recording to a non existing result file the name of that
    # file is in "record_file".
    if (defined $tinfo->{'record_file'}) {
      mtr_add_arg($args, "--result-file=%s", $tinfo->{record_file});
    }
  } else {
    $ENV{'MTR_RECORD'} = 0;
  }

  if ($opt_client_gdb) {
    gdb_arguments(\$args, \$exe, "client");
  } elsif ($opt_client_lldb) {
    lldb_arguments(\$args, \$exe, "client");
  } elsif ($opt_client_ddd) {
    ddd_arguments(\$args, \$exe, "client");
  } elsif ($opt_client_dbx) {
    dbx_arguments(\$args, \$exe, "client");
  } elsif ($opt_client_debugger) {
    debugger_arguments(\$args, \$exe, "client");
  }

  my @redirect_output;
  if ($opt_check_testcases && !defined $tinfo->{'result_file'}) {
    @redirect_output = (output => "/dev/null");
  }

  my $proc = My::SafeProcess->new(name   => "mysqltest",
                                  path   => $exe,
                                  args   => \$args,
                                  append => 1,
                                  @redirect_output,
                                  error   => $path_current_testlog,
                                  verbose => $opt_verbose,);
  mtr_verbose("Started $proc");
  return $proc;
}

sub create_debug_statement {
  my $args  = shift;
  my $input = shift;

  # Put arguments into a single string and enclose values which
  # contain metacharacters in quotes
  my $runline;
  for my $arg (@$$args) {
    $runline .= ($arg =~ /^(--[a-z0-9_-]+=)(.*[^A-Za-z_0-9].*)$/ ? "$1\"$2\"" :
                   $arg
      ) .
      " ";
  }

  $runline = $input ? "run $runline < $input" : "run $runline";
  return $runline;
}

# Modify the exe and args so that program is run in gdb in xterm
sub gdb_arguments {
  my $args  = shift;
  my $exe   = shift;
  my $type  = shift;
  my $input = shift;

  my $gdb_init_file = "$opt_vardir/tmp/gdbinit.$type";

  # Remove the old gdbinit file
  unlink($gdb_init_file);

  my $runline = create_debug_statement($args, $input);

  # write init file for mysqld or client
  mtr_tofile($gdb_init_file, "break main\n" . $runline);

  if ($opt_manual_gdb || $opt_manual_boot_gdb) {
    print "\nTo start gdb for $type, type in another window:\n";
    print "gdb -cd $glob_mysql_test_dir -x $gdb_init_file $$exe\n";

    # Indicate the exe should not be started
    $$exe = undef;
    return;
  }

  $$args = [];
  mtr_add_arg($$args, "-title");
  mtr_add_arg($$args, "$type");
  mtr_add_arg($$args, "-e");

  if ($exe_libtool) {
    mtr_add_arg($$args, $exe_libtool);
    mtr_add_arg($$args, "--mode=execute");
  }

  mtr_add_arg($$args, "gdb");
  mtr_add_arg($$args, "-x");
  mtr_add_arg($$args, "$gdb_init_file");
  mtr_add_arg($$args, "$$exe");

  $$exe = "xterm";
}

# Modify the exe and args so that program is run in lldb
sub lldb_arguments {
  my $args  = shift;
  my $exe   = shift;
  my $type  = shift;
  my $input = shift;

  my $lldb_init_file = "$opt_vardir/tmp/$type.lldbinit";
  unlink($lldb_init_file);

  my $str = join(" ", @$$args);

  # write init file for mysqld or client
  mtr_tofile($lldb_init_file, "process launch --stop-at-entry -- " . $str);

  if ($opt_manual_lldb) {
    print "\nTo start lldb for $type, type in another window:\n";
    print "cd $glob_mysql_test_dir && lldb -s $lldb_init_file $$exe\n";

    # Indicate the exe should not be started
    $$exe = undef;
    return;
  }

  $$args = [];
  mtr_add_arg($$args, "-title");
  mtr_add_arg($$args, "$type");
  mtr_add_arg($$args, "-e");

  mtr_add_arg($$args, "lldb");
  mtr_add_arg($$args, "-s");
  mtr_add_arg($$args, "$lldb_init_file");
  mtr_add_arg($$args, "$$exe");

  $$exe = "xterm";
}

# Modify the exe and args so that program is run in ddd
sub ddd_arguments {
  my $args  = shift;
  my $exe   = shift;
  my $type  = shift;
  my $input = shift;

  my $gdb_init_file = "$opt_vardir/tmp/gdbinit.$type";

  # Remove the old gdbinit file
  unlink($gdb_init_file);

  my $runline = create_debug_statement($args, $input);

  # Write init file for mysqld or client
  mtr_tofile($gdb_init_file, "file $$exe\n" . "break main\n" . $runline);

  if ($opt_manual_ddd) {
    print "\nTo start ddd for $type, type in another window:\n";
    print "ddd -cd $glob_mysql_test_dir -x $gdb_init_file $$exe\n";

    # Indicate the exe should not be started
    $$exe = undef;
    return;
  }

  my $save_exe = $$exe;
  $$args = [];
  if ($exe_libtool) {
    $$exe = $exe_libtool;
    mtr_add_arg($$args, "--mode=execute");
    mtr_add_arg($$args, "ddd");
  } else {
    $$exe = "ddd";
  }
  mtr_add_arg($$args, "--command=$gdb_init_file");
  mtr_add_arg($$args, "$save_exe");
}

# Modify the exe and args so that program is run in dbx in xterm
sub dbx_arguments {
  my $args  = shift;
  my $exe   = shift;
  my $type  = shift;
  my $input = shift;

  # Put $args into a single string
  my $str = join " ", @$$args;
  my $runline = $input ? "run $str < $input" : "run $str";

  if ($opt_manual_dbx) {
    print "\nTo start dbx for $type, type in another window:\n";
    print "cd $glob_mysql_test_dir; dbx -c \"stop in main; " .
      "$runline\" $$exe\n";

    # Indicate the exe should not be started
    $$exe = undef;
    return;
  }

  $$args = [];
  mtr_add_arg($$args, "-title");
  mtr_add_arg($$args, "$type");
  mtr_add_arg($$args, "-e");

  if ($exe_libtool) {
    mtr_add_arg($$args, $exe_libtool);
    mtr_add_arg($$args, "--mode=execute");
  }

  mtr_add_arg($$args, "dbx");
  mtr_add_arg($$args, "-c");
  mtr_add_arg($$args, "stop in main; $runline");
  mtr_add_arg($$args, "$$exe");

  $$exe = "xterm";
}

# Modify the exe and args so that program is run in the selected debugger
sub debugger_arguments {
  my $args     = shift;
  my $exe      = shift;
  my $debugger = $opt_debugger || $opt_client_debugger;

  if ($debugger =~ /vcexpress|vc|devenv/) {
    # vc[express] /debugexe exe arg1 .. argn, add name of the exe
    # and /debugexe before args
    unshift(@$$args, "$$exe");
    unshift(@$$args, "/debugexe");

    # Set exe to debuggername
    $$exe = $debugger;

  } elsif ($debugger =~ /windbg/) {
    # windbg exe arg1 .. argn, add name of the exe before args
    unshift(@$$args, "$$exe");

    # Set exe to debuggername
    $$exe = $debugger;

  } else {
    mtr_error("Unknown argument \"$debugger\" passed to --debugger");
  }
}

# Modify the exe and args so that program is run in strace
sub strace_server_arguments {
  my $args = shift;
  my $exe  = shift;
  my $type = shift;

  mtr_add_arg($args, "-o");
  mtr_add_arg($args, "%s/log/%s.strace", $opt_vardir, $type);
  mtr_add_arg($args, "-f");
  mtr_add_arg($args, $$exe);
  $$exe = "strace";
}

# Modify the exe and args so that client program is run in valgrind
sub valgrind_client_arguments {
  my $args = shift;
  my $exe  = shift;

  mtr_add_arg($args, "--tool=memcheck");
  mtr_add_arg($args, "--quiet");
  mtr_add_arg($args, "--leak-check=full");
  mtr_add_arg($args, "--show-leak-kinds=definite,indirect");
  mtr_add_arg($args, "--errors-for-leak-kinds=definite,indirect");
  mtr_add_arg($args, "--num-callers=16");
  mtr_add_arg($args, "--error-exitcode=42");
  mtr_add_arg($args, "--suppressions=%s/valgrind.supp", $glob_mysql_test_dir)
    if -f "$glob_mysql_test_dir/valgrind.supp";

  mtr_add_arg($args, $$exe);
  $$exe = $opt_valgrind_path || "valgrind";
}

# Modify the exe and args so that program is run in valgrind
sub valgrind_arguments {
  my $args          = shift;
  my $exe           = shift;
  my $report_prefix = shift;

  my @tool_list = grep(/^--tool=(memcheck|callgrind|massif)/, @valgrind_args);

  if (@tool_list) {
    # Get the value of the last specified --tool=<> argument to valgrind
    my ($tool_name) = $tool_list[-1] =~ /(memcheck|callgrind|massif)$/;
    if ($tool_name =~ /memcheck/) {
      $daemonize_mysqld ? mtr_add_arg($args, "--leak-check=no") :
        mtr_add_arg($args, "--leak-check=yes");
    } else {
      $$exe =~ /.*[\/](.*)$/;
      my $report_prefix = defined $report_prefix ? $report_prefix : $1;
      mtr_add_arg($args,
                  "--$tool_name-out-file=$opt_vardir/log/" .
                    "$report_prefix" . "_$tool_name.out.%%p");
    }
  }

  # Add valgrind options, can be overriden by user
  mtr_add_arg($args, '%s', $_) for (@valgrind_args);

  # Non-zero exit code, to ensure failure is reported.
  mtr_add_arg($args, "--error-exitcode=42");

  mtr_add_arg($args, $$exe);

  $$exe = $opt_valgrind_path || "valgrind";

  if ($exe_libtool) {
    # Add "libtool --mode-execute" before the test to execute
    # if running in valgrind(to avoid valgrinding bash)
    unshift(@$args, "--mode=execute", $$exe);
    $$exe = $exe_libtool;
  }
}

# Search server logs for valgrind reports printed at mysqld termination
# Also search for sanitize reports.
sub valgrind_exit_reports() {
  my $found_err = 0;

  foreach my $log_file (keys %mysqld_logs) {
    my @culprits      = ();
    my $valgrind_rep  = "";
    my $found_report  = 0;
    my $err_in_report = 0;
    my $ignore_report = 0;
    my $tool_name     = $opt_sanitize ? "Sanitizer" : "Valgrind";

    my $LOGF = IO::File->new($log_file) or
      mtr_error("Could not open file '$log_file' for reading: $!");

    while (my $line = <$LOGF>) {
      if ($line =~ /^CURRENT_TEST: (.+)$/) {
        my $testname = $1;
        # If we have a report, report it if needed and start new list of tests
        if ($found_report) {
          if ($err_in_report) {
            mtr_print("$tool_name report from $log_file after tests:\n",
                      @culprits);
            mtr_print_line();
            print("$valgrind_rep\n");
            $found_err     = 1;
            $err_in_report = 0;
          }
          # Make ready to collect new report
          @culprits     = ();
          $found_report = 0;
          $valgrind_rep = "";
        }
        push(@culprits, $testname);
        next;
      }

      # This line marks a report to be ignored
      $ignore_report = 1 if $line =~ /VALGRIND_DO_QUICK_LEAK_CHECK/;

      # This line marks the start of a valgrind report
      $found_report = 1 if $line =~ /^==\d+== .* SUMMARY:/;

      # This line marks the start of UBSAN memory leaks
      $found_report = 1 if $line =~ /^==\d+==ERROR:.*/;

      # Various UBSAN runtime errors
      $found_report = 1 if $line =~ /.*runtime error: .*/;

      # TSAN errors
      $found_report = 1 if $line =~ /^WARNING: ThreadSanitizer: .*/;

      if ($ignore_report && $found_report) {
        $ignore_report = 0;
        $found_report  = 0;
      }

      if ($found_report) {
        $line =~ s/^==\d+== //;
        $valgrind_rep .= $line;
        $err_in_report = 1 if $line =~ /ERROR SUMMARY: [1-9]/;
        $err_in_report = 1 if $line =~ /^==\d+==ERROR:.*/;
        $err_in_report = 1 if $line =~ /definitely lost: [1-9]/;
        $err_in_report = 1 if $line =~ /possibly lost: [1-9]/;
        $err_in_report = 1 if $line =~ /still reachable: [1-9]/;
        $err_in_report = 1 if $line =~ /.*runtime error: .*/;
        $err_in_report = 1 if $line =~ /^WARNING: ThreadSanitizer: .*/;
      }
    }

    $LOGF = undef;

    if ($err_in_report) {
      mtr_print("$tool_name report from $log_file after tests:\n", @culprits);
      mtr_print_line();
      print("$valgrind_rep\n");
      $found_err = 1;
    }
  }

  return $found_err;
}

sub run_ctest() {
  my $olddir = getcwd();
  chdir($bindir) or die("Could not chdir to $bindir");

  my $tinfo;
  my $no_ctest = (IS_WINDOWS) ? 256 : -1;
  my $ctest_vs = "";

  # Just ignore if not configured/built to run ctest
  if (!-f "CTestTestfile.cmake") {
    mtr_report("No unit tests found.");
    chdir($olddir);
    return;
  }

  # Add vs-config option if needed
  $ctest_vs = "-C $opt_vs_config" if $opt_vs_config;

  # Also silently ignore if we don't have ctest and didn't insist.
  # Special override: also ignore in Pushbuild, some platforms may
  # not have it. Now, run ctest and collect output.
  $ENV{CTEST_OUTPUT_ON_FAILURE} = 1;
  my $ctest_out = `ctest --test-timeout $opt_ctest_timeout $ctest_vs 2>&1`;
  if ($? == $no_ctest && ($opt_ctest == -1 || defined $ENV{PB2WORKDIR})) {
    chdir($olddir);
    return;
  }

  # Create minimalistic "test" for the reporting
  $tinfo = My::Test->new(name      => 'unit_tests',
                         shortname => 'unit_tests',);

  # Set dummy worker id to align report with normal tests
  $tinfo->{worker} = 0 if $opt_parallel > 1;

  my $ctfail = 0;    # Did ctest fail?
  if ($?) {
    $ctfail           = 1;
    $tinfo->{result}  = 'MTR_RES_FAILED';
    $tinfo->{comment} = "ctest failed with exit code $?, see result below";
    $ctest_out        = "" unless $ctest_out;
  }

  my $ctfile = "$opt_vardir/ctest.log";
  my $ctres  = 0;                         # Did ctest produce report summary?

  open(CTEST, " > $ctfile") or die("Could not open output file $ctfile");

  $ctest_report .= $ctest_out if $opt_ctest_report;

  # Put ctest output in log file, while analyzing results
  for (split('\n', $ctest_out)) {
    print CTEST "$_\n";
    if (/tests passed/) {
      $ctres = 1;
      $ctest_report .= "\nUnit tests: $_\n";
    }

    if (/FAILED/ or /\(Failed\)/) {
      $ctfail = 1;
      $ctest_report .= "  $_\n";
    }
  }
  close CTEST;

  # Set needed 'attributes' for test reporting
  $tinfo->{comment} .= "\nctest did not pruduce report summary" if !$ctres;
  $tinfo->{result} = ($ctres && !$ctfail) ? 'MTR_RES_PASSED' : 'MTR_RES_FAILED';
  $ctest_report .= "Report from unit tests in $ctfile";
  $tinfo->{failures} = ($tinfo->{result} eq 'MTR_RES_FAILED');

  mark_time_used('test');
  mtr_report_test($tinfo);
  chdir($olddir);
  return $tinfo;
}

# Usage
sub usage ($) {
  my ($message) = @_;

  if ($message) {
    print STDERR "$message\n";
  }

  print <<HERE;

$0 [ OPTIONS ] [ TESTCASE ]

Options to control what engine/variation to run

  combination=<opt>     Use at least twice to run tests with specified
                        options to mysqld.
  compress              Use the compressed protocol between client and server.
  cursor-protocol       Use the cursor protocol between client and server
                        (implies --ps-protocol).
  defaults-extra-file=<config template>
                        Extra config template to add to all generated configs.
  defaults-file=<config template>
                        Use fixed config template for all tests.
  explain-protocol      Run 'EXPLAIN EXTENDED' on all SELECT, INSERT,
                        REPLACE, UPDATE and DELETE queries.
  json-explain-protocol Run 'EXPLAIN FORMAT=JSON' on all SELECT, INSERT,
                        REPLACE, UPDATE and DELETE queries.
  opt-trace-protocol    Print optimizer trace.
  ps-protocol           Use the binary protocol between client and server.
  skip-combinations     Ignore combination file (or options).
  skip-ssl              Dont start server with support for ssl connections.
  sp-protocol           Create a stored procedure to execute all queries.
  ssl                   Use ssl protocol between client and server.
  view-protocol         Create a view to execute all non updating queries.
  vs-config             Visual Studio configuration used to create executables
                        (default: MTR_VS_CONFIG environment variable).

Options to control directories to use

  clean-vardir          Clean vardir if tests were successful and if running in
                        "memory". Otherwise this option is ignored.
  client-bindir=PATH    Path to the directory where client binaries are located.
  client-libdir=PATH    Path to the directory where client libraries are
                        located.
  mem                   Run testsuite in "memory" using tmpfs or ramdisk.
                        Attempts to find a suitable location using a builtin
                        list of standard locations for tmpfs (/dev/shm,
                        /run/shm, /tmp). The option can also be set using
                        an environment variable MTR_MEM=[DIR].
  tmpdir=DIR            The directory where temporary files are stored
                        (default: ./var/tmp).
  vardir=DIR            The directory where files generated from the test run
                        is stored (default: ./var). Specifying a ramdisk or
                        tmpfs will speed up tests.

Options to control what test suites or cases to run

  big-test              Also run tests marked as "big".
  do-suite=PREFIX or REGEX
                        Run tests from suites whose name is prefixed with
                        PREFIX or fulfills REGEX.
  do-test-list=FILE     Run the tests listed in FILE. The tests should be
                        listed one per line in the file. "#" as first
                        character marks a comment and is ignored. Similary
                        an empty line in the file is also ignored.
  do-test=PREFIX or REGEX
                        Run test cases which name are prefixed with PREFIX
                        or fulfills REGEX.
  enable-disabled       Run also tests marked as disabled.
  force                 Continue to run the suite after failure.
  include-ndb[cluster]  Enable all tests that need cluster.
  only-big-test         Run only big tests and skip the normal(non-big) tests.
  print-testcases       Don't run the tests but print details about all the
                        selected tests, in the order they would be run.
  skip-ndb[cluster]     Skip all tests that need cluster. This setting is
                        enabled by default.
  skip-rpl              Skip the replication test cases.
  skip-sys-schema       Skip loading of the sys schema, and running the
                        sysschema test suite. An empty sys database is
                        still created.
  skip-test-list=FILE   Skip the tests listed in FILE. Each line in the file
                        is an entry and should be formatted as: 
                        <TESTNAME> : <COMMENT>
  skip-test=PREFIX or REGEX
                        Skip test cases which name are prefixed with PREFIX
                        or fulfills REGEX.
  start-from=PREFIX     Run test cases starting test prefixed with PREFIX where
                        prefix may be suite.testname or just testname.
  suite[s]=NAME1,..,NAMEN
                        Collect tests in suites from the comma separated
                        list of suite names. The default is "$DEFAULT_SUITES".
  with-ndbcluster-only  Run only tests that include "ndb" in the filename.

Options that specify ports

  build-thread=#        Can be set in environment variable MTR_BUILD_THREAD.
                        Set MTR_BUILD_THREAD="auto" to automatically aquire
                        a build thread id that is unique to current host.
  mtr-build-thread=#    Specify unique number to calculate port number(s) from.
  mtr-port-base=#       Base for port numbers.
  mysqlx-port           Specify the port number to be used for mysqlxplugin.
                        Can be set in environment variable MYSQLXPLUGIN_PORT.
                        If not specified will create its own ports. This option
                        will not work for parallel servers.
  port-base=#           number+9 are reserved. Should be divisible by 10, if not
                        it will be rounded down. Value can be set with
                        environment variable MTR_PORT_BASE. If this value is set
                        and is not "auto", it overrides build-thread.

Options for test case authoring

  check-testcases       Check testcases for side effects. If there is any
                        difference in system state before and after the test
                        run, the test case is marked as failed. When this option
                        is enabled, MTR does additional check for missing
                        '.result' file and a test case not having its
                        corresponding '.result' file is marked as failed.
                        To disable this check, use '--nocheck-testcases' option.
  mark-progress         Log line number and elapsed time to <testname>.progress
                        file.
  record TESTNAME       (Re)genereate the result file for TESTNAME.
  test-progress         Print the percentage of tests completed.

Options that pass on options (these may be repeated)

  mysqld=ARGS           Specify additional arguments to "mysqld".
  mysqld-env=VAR=VAL    Specify additional environment settings for "mysqld".

Options for mysqltest
  mysqltest=ARGS        Extra options used when running test clients.

Options to run test on running server

  extern option=value   Run only the tests against an already started server
                        the options to use for connection to the extern server
                        must be specified using name-value pair notation.
                        For example:
                         ./$0 --extern socket=/tmp/mysqld.sock

Options for debugging the product

  boot-dbx              Start bootstrap server in dbx.
  boot-ddd              Start bootstrap server in ddd.
  boot-gdb              Start bootstrap server in gdb.
  client-dbx            Start mysqltest client in dbx.
  client-ddd            Start mysqltest client in ddd.
  client-debugger=NAME  Start mysqltest in the selected debugger.
  client-gdb            Start mysqltest client in gdb.
  client-lldb           Start mysqltest client in lldb.
  dbx                   Start the mysqld(s) in dbx.
  ddd                   Start the mysqld(s) in ddd.
  debug                 Dump trace output for all servers and client programs.
  debug-common          Same as debug, but sets 'd' debug flags to
                        "query,info,error,enter,exit"; you need this if you
                        want both to see debug printouts and to use
                        DBUG_EXECUTE_IF.
  debug-server          Use debug version of server, but without turning on
                        tracing.
  debugger=NAME         Start mysqld in the selected debugger.
  gdb                   Start the mysqld(s) in gdb.
  lldb                  Start the mysqld(s) in lldb.
  manual-boot-gdb       Let user manually start mysqld in gdb, during initialize
                        process.
  manual-dbx            Let user manually start mysqld in dbx, before running
                        test(s).
  manual-ddd            Let user manually start mysqld in ddd, before running
                        test(s).
  manual-debug          Let user manually start mysqld in debugger, before
                        running test(s).
  manual-gdb            Let user manually start mysqld in gdb, before running
                        test(s).
  manual-lldb           Let user manually start mysqld in lldb, before running
                        test(s).
  max-save-core         Limit the number of core files saved (to avoid filling
                        up disks for heavily crashing server). Defaults to
                        $opt_max_save_core, set to 0 for no limit. Set it's
                        default with MTR_MAX_SAVE_CORE.
  max-save-datadir      Limit the number of datadir saved (to avoid filling up
                        disks for heavily crashing server). Defaults to
                        $opt_max_save_datadir, set to 0 for no limit. Set it's
                        default with MTR_MAX_SAVE_DATDIR.
  max-test-fail         Limit the number of test failurs before aborting the
                        current test run. Defaults to $opt_max_test_fail, set to
                        0 for no limit. Set it's default with MTR_MAX_TEST_FAIL.
  strace-client         Create strace output for mysqltest client.
  strace-server         Create strace output for mysqltest server.

Options for valgrind

  callgrind             Instruct valgrind to use callgrind.
  helgrind              Instruct valgrind to use helgrind.
  valgrind              Run the "mysqltest" and "mysqld" executables using
                        valgrind with default options.
  valgrind-all          Synonym for --valgrind.
  valgrind-clients      Run clients started by .test files with valgrind.
  valgrind-mysqld       Run the "mysqld" executable with valgrind.
  valgrind-mysqltest    Run the "mysqltest" and "mysql_client_test" executable
                        with valgrind.
  valgrind-option=ARGS  Option to give valgrind, replaces default option(s), can
                        be specified more then once.
  valgrind-options=ARGS Deprecated, use --valgrind-option.
  valgrind-path=<EXE>   Path to the valgrind executable.

Misc options

  charset-for-testdb    CREATE DATABASE test CHARACTER SET <option value>.
  colored-diff          Colorize the diff part of the output.
  comment=STR           Write STR to the output.
  debug-sync-timeout=NUM
                        Set default timeout for WAIT_FOR debug sync
                        actions. Disable facility with NUM=0.
  default-myisam        Set default storage engine to MyISAM for non-innodb
                        tests. This is needed after switching default storage
                        engine to InnoDB.
  disk-usage            Show disk usage of vardir after each test.
  experimental=<file>   Refer to list of tests considered experimental.
                        Failures will be marked exp-fail instead of fail.
  fast                  Run as fast as possible, dont't wait for servers
                        to shutdown etc.
  force-restart         Always restart servers between tests.
  gcov                  Collect coverage information after the test.
                        The result is a gcov file per source and header file.
  gprof                 Collect profiling information using gprof.
  help                  Get this help text.
  max-connections=N     Max number of open connection to server in mysqltest.
  no-skip               This option is used to run all MTR tests even if the
                        condition required for running the test as specified
                        by inc files are not satisfied. The option mandatorily
                        requires an excluded list at include/excludenoskip.list
                        which contains inc files which should continue to skip.
  non-parallel-test     Also run tests marked as 'non-parallel'. Tests sourcing
                        'not_parallel.inc' are marked as 'non-parallel' tests.
  nounit-tests          Do not run unit tests. Normally run if configured
                        and if not running named tests/suites.
  parallel=N            Run tests in N parallel threads. The default value is
                        "auto", which is equal to the number of CPUs in the
                        machine.
                        Use parallel=auto for auto-setting of N.
  reorder               Reorder tests to get fewer server restarts.
  repeat=N              Run each test N number of times, in parallel if
                        --parallel option value is > 1.
  report-features       First run a "test" that reports mysql features.
  report-times          Report how much time has been spent on different.
  report-unstable-tests Mark tests which fail initially but pass on at least
                        one retry attempt as unstable tests and report them
                        separately in the end summary. If all failures
                        encountered are due to unstable tests, MTR will print
                        a warning and exit with a zero status code.
  retry-failure=N       Limit number of retries for a failed test.
  retry=N               Retry tests that fail N times, limit number of failures
                        to $opt_retry_failure.
  sanitize              Scan server log files for warnings from various
                        sanitizers. Assumes that you have built with
                        -DWITH_ASAN or -DWITH_UBSAN.
  shutdown-timeout=SECONDS
                        Max number of seconds to wait for server shutdown
                        before killing servers (default $opt_shutdown_timeout).
  sleep=SECONDS         Passed to mysqltest, will be used as fixed sleep time
  start                 Only initialize and start the servers. If a testcase is
                        mentioned server is started with startup settings of the
                        testcase. If a --suite option is specified the
                        configurations of the my.cnf of the specified suite is
                        used. If no suite or testcase is mentioned, settings
                        from include/default_my.cnf is used.
                        Example:
                          $0 --start alias &
  start-and-exit        Same as --start, but mysql-test-run terminates and
                        leaves just the server running.
  start-dirty           Only start the servers (without initialization) for
                        the first specified test case.
  stress=ARGS           Run stress test, providing options to
                        mysql-stress-test.pl. Options are separated by comma.
  suite-opt             Run the particular file in the suite as the suite.opt.
  suite-timeout=MINUTES Max test suite run time (default $opt_suite_timeout).
  summary-report=FILE   Generate a plain text file of the test summary only,
                        suitable for sending by email.
  testcase-timeout=MINUTES
                        Max test case run time (default $opt_testcase_timeout).
  timediff              With --timestamp, also print time passed since
                        *previous* test started.
  timer                 Show test case execution time.
  timestamp             Print timestamp before each test report line.
  unit-tests            Run unit tests even if they would otherwise not be run.
  unit-tests-report     Include report of every test included in unit tests.
  user-args             In combination with start* and no test name, drops
                        arguments to mysqld except those specified with
                        --mysqld (if any).
  user=USER             User for connecting to mysqld(default: $opt_user).
  verbose               More verbose output.
  verbose-restart       Write when and why servers are restarted.
  wait-all              If --start or --start-dirty option is used, wait for all
                        servers to exit before finishing the process.
  warnings              Scan the log files for warnings. Use --nowarnings
                        to turn off.
  xml-report=FILE       Generate a XML report file compatible with JUnit.

Some options that control enabling a feature for normal test runs,
can be turned off by prepending 'no' to the option, e.g. --notimer.
This applies to reorder, timer, check-testcases and warnings.

HERE
  exit(1);

}

sub list_options ($) {
  my $hash = shift;

  for (keys %$hash) {
    s/([:=].*|[+!])$//;
    s/\|/\n--/g;
    print "--$_\n" unless /list-options/;
  }

  exit(1);
}
