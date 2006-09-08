#!/usr/bin/perl
# -*- cperl -*-

# This is a transformation of the "mysql-test-run" Bourne shell script
# to Perl. There are reasons this rewrite is not the prettiest Perl
# you have seen
#
#   - The original script is huge and for most part uncommented,
#     not even a usage description of the flags.
#
#   - There has been an attempt to write a replacement in C for the
#     original Bourne shell script. It was kind of working but lacked
#     lot of functionality to really be a replacement. Not to redo
#     that mistake and catch all the obscure features of the original
#     script, the rewrite in Perl is more close to the original script
#     meaning it also share some of the ugly parts as well.
#
#   - The original intention was that this script was to be a prototype
#     to be the base for a new C version with full functionality. Since
#     then it was decided that the Perl version should replace the
#     Bourne shell version, but the Perl style still reflects the wish
#     to make the Perl to C step easy.
#
# Some coding style from the original intent has been kept
#
#   - To make this Perl script easy to alter even for those that not
#     code Perl that often, the coding style is as close as possible to
#     the C/C++ MySQL coding standard.
#
#   - All lists of arguments to send to commands are Perl lists/arrays,
#     not strings we append args to. Within reason, most string
#     concatenation for arguments should be avoided.
#
#   - Functions defined in the main program are not to be prefixed,
#     functions in "library files" are to be prefixed with "mtr_" (for
#     Mysql-Test-Run). There are some exceptions, code that fits best in
#     the main program, but are put into separate files to avoid
#     clutter, may be without prefix.
#
#   - All stat/opendir/-f/ is to be kept in collect_test_cases(). It
#     will create a struct that the rest of the program can use to get
#     the information. This separates the "find information" from the
#     "do the work" and makes the program more easy to maintain.
#
#   - At the moment, there are tons of "global" variables that control
#     this script, even accessed from the files in "lib/*.pl". This
#     will change over time, for now global variables are used instead
#     of using %opt, %path and %exe hashes, because I want more
#     compile time checking, that hashes would not give me. Once this
#     script is debugged, hashes will be used and passed as parameters
#     to functions, to more closely mimic how it would be coded in C
#     using structs.
#
#   - The rule when it comes to the logic of this program is
#
#       command_line_setup() - is to handle the logic between flags
#       collect_test_cases() - is to do its best to select what tests
#                              to run, dig out options, if needs restart etc.
#       run_testcase()       - is to run a single testcase, and follow the
#                              logic set in both above. No, or rare file
#                              system operations. If a test seems complex,
#                              it should probably not be here.
#
# A nice way to trace the execution of this script while debugging
# is to use the Devel::Trace package found at
# "http://www.plover.com/~mjd/perl/Trace/" and run this script like
# "perl -d:Trace mysql-test-run.pl"
#
# FIXME Save a PID file from this code as well, to record the process
#       id we think it has. In Cygwin, a fork creates one Cygwin process,
#       and then the real Win32 process. Cygwin Perl can only kill Cygwin
#       processes. And "mysqld --bootstrap ..." doesn't save a PID file.

$Devel::Trace::TRACE= 0;       # Don't trace boring init stuff

#require 5.6.1;
use File::Path;
use File::Basename;
use File::Copy;
use Cwd;
use Getopt::Long;
use Sys::Hostname;
#use Carp;
use IO::Socket;
use IO::Socket::INET;
use Data::Dumper;
use strict;
#use diagnostics;

require "lib/mtr_cases.pl";
require "lib/mtr_process.pl";
require "lib/mtr_timer.pl";
require "lib/mtr_io.pl";
require "lib/mtr_gcov.pl";
require "lib/mtr_gprof.pl";
require "lib/mtr_report.pl";
require "lib/mtr_diff.pl";
require "lib/mtr_match.pl";
require "lib/mtr_misc.pl";
require "lib/mtr_stress.pl";

$Devel::Trace::TRACE= 1;

# Used by gcov
our @mysqld_src_dirs=
  (
   "strings",
   "mysys",
   "include",
   "extra",
   "regex",
   "isam",
   "merge",
   "myisam",
   "myisammrg",
   "heap",
   "sql",
  );

##############################################################################
#
#  Default settings
#
##############################################################################

# We are to use handle_options() in "mysys/my_getopt.c" for the C version
#
# In the C version we want to use structs and, in some cases, arrays of
# structs. We let each struct be a separate hash.

# Misc global variables

our $glob_win32=                  0; # OS and native Win32 executables
our $glob_win32_perl=             0; # ActiveState Win32 Perl
our $glob_cygwin_perl=            0; # Cygwin Perl
our $glob_cygwin_shell=           undef;
our $glob_mysql_test_dir=         undef;
our $glob_mysql_bench_dir=        undef;
our $glob_hostname=               undef;
our $glob_scriptname=             undef;
our $glob_timers=                 undef;
our $glob_use_running_server=     0;
our $glob_use_running_ndbcluster= 0;
our $glob_use_running_ndbcluster_slave= 0;
our $glob_use_embedded_server=    0;
our @glob_test_mode;

our $glob_basedir;

# The total result

our $path_charsetsdir;
our $path_client_bindir;
our $path_language;
our $path_timefile;
our $path_snapshot;
our $path_slave_load_tmpdir;     # What is this?!
our $path_mysqltest_log;
our $path_current_test_log;
our $path_my_basedir;
our $opt_vardir;                 # A path but set directly on cmd line
our $opt_vardir_trace;           # unix formatted opt_vardir for trace files
our $opt_tmpdir;                 # A path but set directly on cmd line

our $opt_usage;
our $opt_suite;

our $opt_netware;

our $opt_script_debug= 0;  # Script debugging, enable with --script-debug
our $opt_verbose= 0;  # Verbose output, enable with --verbose

# Options FIXME not all....

our $exe_master_mysqld;
our $exe_mysql;
our $exe_mysqladmin;
our $exe_mysqlbinlog;
our $exe_mysql_client_test;
our $exe_mysqld;
our $exe_mysqlcheck;             # Called from test case
our $exe_mysqldump;              # Called from test case
our $exe_mysqlslap;              # Called from test case
our $exe_mysqlimport;              # Called from test case
our $exe_mysqlshow;              # Called from test case
our $exe_mysql_fix_system_tables;
our $exe_mysqltest;
our $exe_ndbd;
our $exe_ndb_mgmd;
our $exe_slave_mysqld;
our $exe_im;
our $exe_my_print_defaults;
our $exe_perror;
our $lib_udf_example;
our $exe_libtool;

our $opt_bench= 0;
our $opt_small_bench= 0;
our $opt_big_test= 0;            # Send --big-test to mysqltest

our @opt_extra_mysqld_opt;

our $opt_compress;
our $opt_ssl;
our $opt_skip_ssl;
our $opt_ssl_supported;
our $opt_ps_protocol;
our $opt_sp_protocol;
our $opt_cursor_protocol;
our $opt_view_protocol;

our $opt_debug;
our $opt_do_test;
our @opt_cases;                  # The test cases names in argv
our $opt_embedded_server;
our $opt_extern;
our $opt_fast;
our $opt_force;
our $opt_reorder= 0;
our $opt_enable_disabled;

our $opt_gcov;
our $opt_gcov_err;
our $opt_gcov_msg;

our $glob_debugger= 0;
our $opt_gdb;
our $opt_client_gdb;
our $opt_ddd;
our $opt_client_ddd;
our $opt_manual_gdb;
our $opt_manual_ddd;
our $opt_manual_debug;
our $opt_debugger;
our $opt_client_debugger;

our $opt_gprof;
our $opt_gprof_dir;
our $opt_gprof_master;
our $opt_gprof_slave;

our $master;                    # Will be struct in C
our $slave;
our $clusters;

our $instance_manager;

our $opt_ndbcluster_port;
our $opt_ndbconnectstring;
our $opt_ndbcluster_port_slave;
our $opt_ndbconnectstring_slave;

our $opt_record;
our $opt_check_testcases;

our $opt_result_ext;

our $opt_skip;
our $opt_skip_rpl;
our $use_slaves;
our $use_innodb;
our $opt_skip_test;
our $opt_skip_im;

our $opt_sleep;

our $opt_sleep_time_after_restart=  1;
our $opt_sleep_time_for_delete=    10;
our $opt_testcase_timeout;
our $opt_suite_timeout;
my  $default_testcase_timeout=     15; # 15 min max
my  $default_suite_timeout=       120; # 2 hours max

our $opt_socket;

our $opt_source_dist;

our $opt_start_and_exit;
our $opt_start_dirty;
our $opt_start_from;

our $opt_strace_client;

our $opt_timer= 1;

our $opt_user;
our $opt_user_test;

our $opt_valgrind= 0;
our $opt_valgrind_mysqld= 0;
our $opt_valgrind_mysqltest= 0;
our $default_valgrind_options= "--show-reachable=yes";
our $opt_valgrind_options;
our $opt_valgrind_path;
our $opt_callgrind;

our $opt_stress=               "";
our $opt_stress_suite=     "main";
our $opt_stress_mode=    "random";
our $opt_stress_threads=        5;
our $opt_stress_test_count=     0;
our $opt_stress_loop_count=     0;
our $opt_stress_test_duration=  0;
our $opt_stress_init_file=     "";
our $opt_stress_test_file=     "";

our $opt_wait_for_master;
our $opt_wait_for_slave;
our $opt_wait_timeout=  10;

our $opt_warnings;

our $opt_udiff;

our $opt_skip_ndbcluster= 0;
our $opt_skip_ndbcluster_slave= 0;
our $opt_with_ndbcluster= 0;
our $opt_with_ndbcluster_only= 0;
our $opt_ndbcluster_supported= 0;
our $opt_ndb_extra_test= 0;
our $opt_skip_master_binlog= 0;
our $opt_skip_slave_binlog= 0;

our $exe_ndb_mgm;
our $exe_ndb_waiter;
our $path_ndb_tools_dir;
our $file_ndb_testrun_log;

our @data_dir_lst;

our $used_binlog_format;
our $debug_compiled_binaries;
our $glob_tot_real_time= 0;

######################################################################
#
#  Function declarations
#
######################################################################

sub main ();
sub initial_setup ();
sub command_line_setup ();
sub snapshot_setup ();
sub executable_setup ();
sub environment_setup ();
sub kill_running_server ();
sub cleanup_stale_files ();
sub check_ssl_support ();
sub check_running_as_root();
sub check_ndbcluster_support ();
sub rm_ndbcluster_tables ($);
sub ndbcluster_start_install ($);
sub ndbcluster_start ($$);
sub ndbcluster_wait_started ($$);
sub mysqld_wait_started($);
sub run_benchmarks ($);
sub initialize_servers ();
sub mysql_install_db ();
sub install_db ($$);
sub copy_install_db ($$);
sub run_testcase ($);
sub run_testcase_stop_servers ($$$);
sub run_testcase_start_servers ($);
sub run_testcase_check_skip_test($);
sub report_failure_and_restart ($);
sub do_before_start_master ($$);
sub do_before_start_slave ($$);
sub ndbd_start ($$$);
sub ndb_mgmd_start ($);
sub mysqld_start ($$$);
sub mysqld_arguments ($$$$$);
sub stop_all_servers ();
sub run_mysqltest ($);
sub usage ($);

######################################################################
#
#  Main program
#
######################################################################

main();

sub main () {

  initial_setup();
  command_line_setup();
  executable_setup();

  check_ndbcluster_support();
  check_ssl_support();
  check_debug_support();

  environment_setup();
  signal_setup();

  if ( $opt_gcov )
  {
    gcov_prepare();
  }

  if ( $opt_gprof )
  {
    gprof_prepare();
  }

  if ( $opt_bench )
  {
    initialize_servers();
    run_benchmarks(shift);      # Shift what? Extra arguments?!
  }
  elsif ( $opt_stress )
  {
    initialize_servers();
    run_stress_test()
  }
  else
  {
    # Figure out which tests we are going to run
    my $tests= collect_test_cases($opt_suite);

    # Turn off NDB and other similar options if no tests use it
    my ($need_ndbcluster,$need_im);
    foreach my $test (@$tests)
    {
      $need_ndbcluster||= $test->{ndb_test};
      $need_im||= $test->{component_id} eq 'im';
      $use_slaves||= $test->{slave_num};
      $use_innodb||= $test->{'innodb_test'};
    }
    $opt_skip_ndbcluster= $opt_skip_ndbcluster_slave= 1
      unless $need_ndbcluster;
    $opt_skip_im= 1 unless $need_im;

    snapshot_setup();
    initialize_servers();

    run_suite($opt_suite, $tests);
  }

  mtr_exit(0);
}

##############################################################################
#
#  Initial setup independent on command line arguments
#
##############################################################################

sub initial_setup () {

  select(STDOUT);
  $| = 1;                       # Make unbuffered

  $glob_scriptname=  basename($0);

  $glob_win32_perl=  ($^O eq "MSWin32");
  $glob_cygwin_perl= ($^O eq "cygwin");
  $glob_win32=       ($glob_win32_perl or $glob_cygwin_perl);

  # We require that we are in the "mysql-test" directory
  # to run mysql-test-run

  if (! -f $glob_scriptname)
  {
    mtr_error("Can't find the location for the mysql-test-run script\n" .
              "Go to to the mysql-test directory and execute the script " .
              "as follows:\n./$glob_scriptname");
  }

  if ( -d "../sql" )
  {
    $opt_source_dist=  1;
  }

  $glob_hostname=  mtr_short_hostname();

  # 'basedir' is always parent of "mysql-test" directory
  $glob_mysql_test_dir=  cwd();
  if ( $glob_cygwin_perl )
  {
    # Windows programs like 'mysqld' needs Windows paths
    $glob_mysql_test_dir= `cygpath -m "$glob_mysql_test_dir"`;
    my $shell= $ENV{'SHELL'} || "/bin/bash";
    $glob_cygwin_shell=   `cygpath -w "$shell"`; # The Windows path c:\...
    chomp($glob_mysql_test_dir);
    chomp($glob_cygwin_shell);
  }
  $glob_basedir=         dirname($glob_mysql_test_dir);
  # Expect mysql-bench to be located adjacent to the source tree, by default
  $glob_mysql_bench_dir= "$glob_basedir/../mysql-bench"
    unless defined $glob_mysql_bench_dir;

  # needs to be same length to test logging (FIXME what???)
  $path_slave_load_tmpdir=  "../../var/tmp";

  $path_my_basedir=
    $opt_source_dist ? $glob_mysql_test_dir : $glob_basedir;

  $glob_timers= mtr_init_timers();
}



##############################################################################
#
#  Default settings
#
##############################################################################

sub command_line_setup () {

  # These are defaults for things that are set on the command line

  $opt_suite=        "main";    # Special default suite
  my $opt_comment;

  my $opt_master_myport=       9306;
  my $opt_slave_myport=        9308;
  $opt_ndbcluster_port=        9310;
  $opt_ndbcluster_port_slave=  9311;
  my $im_port=                 9312;
  my $im_mysqld1_port=         9313;
  my $im_mysqld2_port=         9314;

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
  # Also note the limiteation of ports we are allowed to hand out. This
  # differs between operating systems and configuration, see
  # http://www.ncftp.com/ncftpd/doc/misc/ephemeral_ports.html
  # But a fairly safe range seems to be 5001 - 32767
  if ( $ENV{'MTR_BUILD_THREAD'} )
  {
    # Up to two masters, up to three slaves
    $opt_master_myport=         $ENV{'MTR_BUILD_THREAD'} * 10 + 10000; # and 1
    $opt_slave_myport=          $opt_master_myport + 2;  # and 3 4
    $opt_ndbcluster_port=       $opt_master_myport + 5;
    $opt_ndbcluster_port_slave= $opt_master_myport + 6;
    $im_port=                   $opt_master_myport + 7;
    $im_mysqld1_port=           $opt_master_myport + 8;
    $im_mysqld2_port=           $opt_master_myport + 9;
  }

  if ( $opt_master_myport < 5001 or $opt_master_myport + 10 >= 32767 )
  {
    mtr_error("MTR_BUILD_THREAD number results in a port",
              "outside 5001 - 32767",
              "($opt_master_myport - $opt_master_myport + 10)");
  }

  # This is needed for test log evaluation in "gen-build-status-page"
  # in all cases where the calling tool does not log the commands
  # directly before it executes them, like "make test-force-pl" in RPM builds.
  print "Logging: $0 ", join(" ", @ARGV), "\n";

  # Read the command line
  # Note: Keep list, and the order, in sync with usage at end of this file

  Getopt::Long::Configure("pass_through");
  GetOptions(
             # Control what engine/variation to run
             'embedded-server'          => \$opt_embedded_server,
             'ps-protocol'              => \$opt_ps_protocol,
             'sp-protocol'              => \$opt_sp_protocol,
             'view-protocol'            => \$opt_view_protocol,
             'cursor-protocol'          => \$opt_cursor_protocol,
             'ssl|with-openssl'         => \$opt_ssl,
             'skip-ssl'                 => \$opt_skip_ssl,
             'compress'                 => \$opt_compress,
             'bench'                    => \$opt_bench,
             'small-bench'              => \$opt_small_bench,

             # Control what test suites or cases to run
             'force'                    => \$opt_force,
             'with-ndbcluster'          => \$opt_with_ndbcluster,
             'with-ndbcluster-only'     => \$opt_with_ndbcluster_only,
             'skip-ndbcluster|skip-ndb' => \$opt_skip_ndbcluster,
             'skip-ndbcluster-slave|skip-ndb-slave'
                                        => \$opt_skip_ndbcluster_slave,
             'ndb-extra-test'           => \$opt_ndb_extra_test,
             'skip-master-binlog'       => \$opt_skip_master_binlog,
             'skip-slave-binlog'        => \$opt_skip_slave_binlog,
             'do-test=s'                => \$opt_do_test,
             'start-from=s'             => \$opt_start_from,
             'suite=s'                  => \$opt_suite,
             'skip-rpl'                 => \$opt_skip_rpl,
             'skip-im'                  => \$opt_skip_im,
             'skip-test=s'              => \$opt_skip_test,
             'big-test'                 => \$opt_big_test,

             # Specify ports
             'master_port=i'            => \$opt_master_myport,
             'slave_port=i'             => \$opt_slave_myport,
             'ndbcluster-port|ndbcluster_port=i' => \$opt_ndbcluster_port,
             'ndbcluster-port-slave=i'  => \$opt_ndbcluster_port_slave,
             'im-port=i'                => \$im_port, # Instance Manager port.
             'im-mysqld1-port=i'        => \$im_mysqld1_port, # Port of mysqld, controlled by IM
             'im-mysqld2-port=i'        => \$im_mysqld2_port, # Port of mysqld, controlled by IM

             # Test case authoring
             'record'                   => \$opt_record,
             'check-testcases'          => \$opt_check_testcases,

             # Extra options used when starting mysqld
             'mysqld=s'                 => \@opt_extra_mysqld_opt,

             # Run test on running server
             'extern'                   => \$opt_extern,
             'ndb-connectstring=s'       => \$opt_ndbconnectstring,
             'ndb-connectstring-slave=s' => \$opt_ndbconnectstring_slave,

             # Debugging
             'gdb'                      => \$opt_gdb,
             'client-gdb'               => \$opt_client_gdb,
             'manual-gdb'               => \$opt_manual_gdb,
             'manual-debug'             => \$opt_manual_debug,
             'ddd'                      => \$opt_ddd,
             'client-ddd'               => \$opt_client_ddd,
	     'debugger=s'               => \$opt_debugger,
	     'client-debugger=s'        => \$opt_client_debugger,
             'strace-client'            => \$opt_strace_client,
             'master-binary=s'          => \$exe_master_mysqld,
             'slave-binary=s'           => \$exe_slave_mysqld,

             # Coverage, profiling etc
             'gcov'                     => \$opt_gcov,
             'gprof'                    => \$opt_gprof,
             'valgrind|valgrind-all'    => \$opt_valgrind,
             'valgrind-mysqltest'       => \$opt_valgrind_mysqltest,
             'valgrind-mysqld'          => \$opt_valgrind_mysqld,
             'valgrind-options=s'       => \$opt_valgrind_options,
             'valgrind-path=s'          => \$opt_valgrind_path,
	     'callgrind'                => \$opt_callgrind,

             # Stress testing 
             'stress'                   => \$opt_stress,
             'stress-suite=s'           => \$opt_stress_suite,
             'stress-threads=i'         => \$opt_stress_threads,
             'stress-test-file=s'       => \$opt_stress_test_file,
             'stress-init-file=s'       => \$opt_stress_init_file,
             'stress-mode=s'            => \$opt_stress_mode,
             'stress-loop-count=i'      => \$opt_stress_loop_count,
             'stress-test-count=i'      => \$opt_stress_test_count,
             'stress-test-duration=i'   => \$opt_stress_test_duration,

	     # Directories
             'tmpdir=s'                 => \$opt_tmpdir,
             'vardir=s'                 => \$opt_vardir,
             'benchdir=s'               => \$glob_mysql_bench_dir,

             # Misc
             'comment=s'                => \$opt_comment,
             'debug'                    => \$opt_debug,
             'fast'                     => \$opt_fast,
             'netware'                  => \$opt_netware,
             'reorder'                  => \$opt_reorder,
             'enable-disabled'          => \$opt_enable_disabled,
             'script-debug'             => \$opt_script_debug,
             'verbose'                  => \$opt_verbose,
             'sleep=i'                  => \$opt_sleep,
             'socket=s'                 => \$opt_socket,
             'start-dirty'              => \$opt_start_dirty,
             'start-and-exit'           => \$opt_start_and_exit,
             'timer!'                   => \$opt_timer,
             'unified-diff|udiff'       => \$opt_udiff,
             'user-test=s'              => \$opt_user_test,
             'user=s'                   => \$opt_user,
             'wait-timeout=i'           => \$opt_wait_timeout,
             'testcase-timeout=i'       => \$opt_testcase_timeout,
             'suite-timeout=i'          => \$opt_suite_timeout,
             'warnings|log-warnings'    => \$opt_warnings,

             'help|h'                   => \$opt_usage,
            ) or usage("Can't read options");

  usage("") if $opt_usage;

  if ( $opt_comment )
  {
    print "\n";
    print '#' x 78, "\n";
    print "# $opt_comment\n";
    print '#' x 78, "\n\n";
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

  # --------------------------------------------------------------------------
  # Find out type of logging that are being used
  # --------------------------------------------------------------------------

  # NOTE if the default binlog format is changed, this has to be changed
  $used_binlog_format= "stmt";
  foreach my $arg ( @opt_extra_mysqld_opt )
  {
    if ( defined mtr_match_substring($arg,"binlog-format=row"))
    {
      $used_binlog_format= "row";
    }
  }
  mtr_report("Using binlog format '$used_binlog_format'");

  # --------------------------------------------------------------------------
  # Set the "var/" directory, as it is the base for everything else
  # --------------------------------------------------------------------------

  if ( ! $opt_vardir )
  {
    $opt_vardir= "$glob_mysql_test_dir/var";
  }
  $opt_vardir_trace= $opt_vardir;
  # Chop off any "c:", DBUG likes a unix path ex: c:/src/... => /src/...
  $opt_vardir_trace=~ s/^\w://;

  # We make the path absolute, as the server will do a chdir() before usage
  unless ( $opt_vardir =~ m,^/, or
           ($glob_win32 and $opt_vardir =~ m,^[a-z]:/,i) )
  {
    # Make absolute path, relative test dir
    $opt_vardir= "$glob_mysql_test_dir/$opt_vardir";
  }

  # --------------------------------------------------------------------------
  # If not set, set these to defaults
  # --------------------------------------------------------------------------

  $opt_tmpdir=       "$opt_vardir/tmp" unless $opt_tmpdir;
  $opt_tmpdir =~ s,/+$,,;       # Remove ending slash if any

  # --------------------------------------------------------------------------
  # Do sanity checks of command line arguments
  # --------------------------------------------------------------------------

  if ( ! $opt_socket )
  {     # FIXME set default before reading options?
#    $opt_socket=  '@MYSQL_UNIX_ADDR@';
    $opt_socket=  "/tmp/mysql.sock"; # FIXME
  }

  # --------------------------------------------------------------------------
  # Look at the command line options and set script flags
  # --------------------------------------------------------------------------

  if ( $opt_record and ! @opt_cases )
  {
    mtr_error("Will not run in record mode without a specific test case");
  }

  if ( $opt_embedded_server )
  {
    $glob_use_embedded_server= 1;
    push(@glob_test_mode, "embedded");
    $opt_skip_rpl= 1;              # We never run replication with embedded
    $opt_skip_ndbcluster= 1;       # Turn off use of NDB cluster
    $opt_skip_ssl= 1;              # Turn off use of SSL

    if ( $opt_extern )
    {
      mtr_error("Can't use --extern with --embedded-server");
    }
  }

  if ( $opt_ps_protocol )
  {
    push(@glob_test_mode, "ps-protocol");
  }

  if ( $opt_with_ndbcluster and $opt_skip_ndbcluster)
  {
    mtr_error("Can't specify both --with-ndbcluster and --skip-ndbcluster");
  }

  if ( $opt_ndbconnectstring )
  {
    $glob_use_running_ndbcluster= 1;
    mtr_error("Can't specify --ndb-connectstring and --skip-ndbcluster")
      if $opt_skip_ndbcluster;
    mtr_error("Can't specify --ndb-connectstring and --ndbcluster-port")
      if $opt_ndbcluster_port;
  }
  else
  {
    # Set default connect string
    $opt_ndbconnectstring= "host=localhost:$opt_ndbcluster_port";
  }

  if ( $opt_ndbconnectstring_slave )
  {
      $glob_use_running_ndbcluster_slave= 1;
      mtr_error("Can't specify ndb-connectstring_slave and " .
		"--skip-ndbcluster-slave")
	if $opt_skip_ndbcluster;
      mtr_error("Can't specify --ndb-connectstring-slave and " .
		"--ndbcluster-port-slave")
	if $opt_ndbcluster_port_slave;
  }
  else
  {
    # Set default connect string
    $opt_ndbconnectstring_slave= "host=localhost:$opt_ndbcluster_port_slave";
  }

  if ( $opt_small_bench )
  {
    $opt_bench=  1;
  }

  if ( $opt_sleep )
  {
    $opt_sleep_time_after_restart= $opt_sleep;
  }

  if ( $opt_gcov and ! $opt_source_dist )
  {
    mtr_error("Coverage test needs the source - please use source dist");
  }

  # Check debug related options
  if ( $opt_gdb || $opt_client_gdb || $opt_ddd || $opt_client_ddd ||
       $opt_manual_gdb || $opt_manual_ddd || $opt_manual_debug ||
       $opt_debugger || $opt_client_debugger )
  {
    # Indicate that we are using debugger
    $glob_debugger= 1;
    # Increase timeouts
    $opt_wait_timeout=  300;
    if ( $opt_extern )
    {
      mtr_error("Can't use --extern when using debugger");
    }
  }

  # Check IM arguments
  if ( $glob_win32 )
  {
    mtr_report("Disable Instance manager - not supported on Windows");
    $opt_skip_im= 1;
  }
  # Check valgrind arguments
  if ( $opt_valgrind or $opt_valgrind_path or defined $opt_valgrind_options)
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
    mtr_report("Turning on valgrind for mysqltest only");
    $opt_valgrind= 1;
  }

  if ( $opt_callgrind )
  {
    mtr_report("Turning on valgrind with callgrind for mysqld(s)");
    $opt_valgrind= 1;
    $opt_valgrind_mysqld= 1;

    # Set special valgrind options unless options passed on command line
    $opt_valgrind_options="--trace-children=yes"
      unless defined $opt_valgrind_options;
  }

  if ( $opt_valgrind )
  {
    # Set valgrind_options to default unless already defined
    $opt_valgrind_options=$default_valgrind_options
      unless defined $opt_valgrind_options;

    mtr_report("Running valgrind with options \"$opt_valgrind_options\"");
  }

  if ( ! $opt_testcase_timeout )
  {
    $opt_testcase_timeout= $default_testcase_timeout;
    $opt_testcase_timeout*= 10 if defined $opt_valgrind;
  }

  if ( ! $opt_suite_timeout )
  {
    $opt_suite_timeout= $default_suite_timeout;
    $opt_suite_timeout*= 4 if defined $opt_valgrind;
  }

  # Increase times to wait for executables to start if using valgrind
  if ( $opt_valgrind )
  {
    $opt_sleep_time_after_restart= 10;
    $opt_sleep_time_for_delete= 60;
  }

  if ( ! $opt_user )
  {
    if ( $glob_use_running_server )
    {
      $opt_user= "test";
    }
    else
    {
      $opt_user= "root"; # We want to do FLUSH xxx commands
    }
  }

  # On QNX, /tmp/dir/master.sock and /tmp/dir//master.sock seem to be
  # considered different, so avoid the extra slash (/) in the socket
  # paths.
  my $sockdir = $opt_tmpdir;
  $sockdir =~ s|/+$||;

  # Put this into a hash, will be a C struct

  $master->[0]=
  {
   type          => "master",
   idx           => 0,
   path_myddir   => "$opt_vardir/master-data",
   path_myerr    => "$opt_vardir/log/master.err",
   path_mylog    => "$opt_vardir/log/master.log",
   path_pid    => "$opt_vardir/run/master.pid",
   path_sock   => "$sockdir/master.sock",
   port   =>  $opt_master_myport,
   start_timeout =>  400, # enough time create innodb tables
   cluster       =>  0, # index in clusters list
   start_opts    => [],
  };

  $master->[1]=
  {
   type          => "master",
   idx           => 1,
   path_myddir   => "$opt_vardir/master1-data",
   path_myerr    => "$opt_vardir/log/master1.err",
   path_mylog    => "$opt_vardir/log/master1.log",
   path_pid    => "$opt_vardir/run/master1.pid",
   path_sock   => "$sockdir/master1.sock",
   port   => $opt_master_myport + 1,
   start_timeout => 400, # enough time create innodb tables
   cluster       =>  0, # index in clusters list
   start_opts    => [],
  };

  $slave->[0]=
  {
   type          => "slave",
   idx           => 0,
   path_myddir   => "$opt_vardir/slave-data",
   path_myerr    => "$opt_vardir/log/slave.err",
   path_mylog    => "$opt_vardir/log/slave.log",
   path_pid    => "$opt_vardir/run/slave.pid",
   path_sock   => "$sockdir/slave.sock",
   port   => $opt_slave_myport,
   start_timeout => 400,

   cluster       =>  1, # index in clusters list
   start_opts    => [],
  };

  $slave->[1]=
  {
   type          => "slave",
   idx           => 1,
   path_myddir   => "$opt_vardir/slave1-data",
   path_myerr    => "$opt_vardir/log/slave1.err",
   path_mylog    => "$opt_vardir/log/slave1.log",
   path_pid    => "$opt_vardir/run/slave1.pid",
   path_sock   => "$sockdir/slave1.sock",
   port   => $opt_slave_myport + 1,
   start_timeout => 300,
   cluster       =>  -1, # index in clusters list
   start_opts    => [],
  };

  $slave->[2]=
  {
   type          => "slave",
   idx           => 2,
   path_myddir   => "$opt_vardir/slave2-data",
   path_myerr    => "$opt_vardir/log/slave2.err",
   path_mylog    => "$opt_vardir/log/slave2.log",
   path_pid    => "$opt_vardir/run/slave2.pid",
   path_sock   => "$sockdir/slave2.sock",
   port   => $opt_slave_myport + 2,
   start_timeout => 300,
   cluster       =>  -1, # index in clusters list
   start_opts    => [],
  };

  $instance_manager=
  {
   path_err =>        "$opt_vardir/log/im.err",
   path_log =>        "$opt_vardir/log/im.log",
   path_pid =>        "$opt_vardir/run/im.pid",
   path_angel_pid =>  "$opt_vardir/run/im.angel.pid",
   path_sock =>       "$sockdir/im.sock",
   port =>            $im_port,
   start_timeout =>   $master->[0]->{'start_timeout'},
   admin_login =>     'im_admin',
   admin_password =>  'im_admin_secret',
   admin_sha1 =>      '*598D51AD2DFF7792045D6DF3DDF9AA1AF737B295',
   password_file =>   "$opt_vardir/im.passwd",
   defaults_file =>   "$opt_vardir/im.cnf",
  };

  $instance_manager->{'instances'}->[0]=
  {
   server_id    => 1,
   port         => $im_mysqld1_port,
   path_datadir => "$opt_vardir/im_mysqld_1.data",
   path_sock    => "$sockdir/mysqld_1.sock",
   path_pid     => "$opt_vardir/run/mysqld_1.pid",
   old_log_format => 1
  };

  $instance_manager->{'instances'}->[1]=
  {
   server_id    => 2,
   port         => $im_mysqld2_port,
   path_datadir => "$opt_vardir/im_mysqld_2.data",
   path_sock    => "$sockdir/mysqld_2.sock",
   path_pid     => "$opt_vardir/run/mysqld_2.pid",
   nonguarded   => 1,
   old_log_format => 1
  };

  my $data_dir= "$opt_vardir/ndbcluster-$opt_ndbcluster_port";
  $clusters->[0]=
  {
   name            => "Master",
   nodes           => 2,
   port            => "$opt_ndbcluster_port",
   data_dir        => "$data_dir",
   connect_string  => "$opt_ndbconnectstring",
   path_pid        => "$data_dir/ndb_3.pid", # Nodes + 1
   pid             => 0, # pid of ndb_mgmd
   installed_ok    => 'NO',
  };

  $data_dir= "$opt_vardir/ndbcluster-$opt_ndbcluster_port_slave";
  $clusters->[1]=
  {
   name            => "Slave",
   nodes           => 1,
   port            => "$opt_ndbcluster_port_slave",
   data_dir        => "$data_dir",
   connect_string  => "$opt_ndbconnectstring_slave",
   path_pid        => "$data_dir/ndb_2.pid", # Nodes + 1
   pid             => 0, # pid of ndb_mgmd
   installed_ok    => 'NO',
  };

  # Init pids of ndbd's
  foreach my $cluster ( @{$clusters} )
  {
    for ( my $idx= 0; $idx < $cluster->{'nodes'}; $idx++ )
    {
      my $nodeid= $idx+1;
      $cluster->{'ndbds'}->[$idx]=
	{
	 pid      => 0,
	 nodeid => $nodeid,
	 path_pid => "$cluster->{'data_dir'}/ndb_${nodeid}.pid",
	 path_fs => "$cluster->{'data_dir'}/ndb_${nodeid}_fs",
	};
    }
  }

  if ( $opt_extern )
  {
    $glob_use_running_server=  1;
    $opt_skip_rpl= 1;                   # We don't run rpl test cases
    $master->[0]->{'path_sock'}=  $opt_socket;
  }

  $path_timefile=  "$opt_vardir/log/mysqltest-time";
  $path_mysqltest_log=  "$opt_vardir/log/mysqltest.log";
  $path_current_test_log= "$opt_vardir/log/current_test";

  $path_snapshot= "$opt_tmpdir/snapshot_$opt_master_myport/";
}

sub snapshot_setup () {

  # Make a list of all data_dirs
  @data_dir_lst = (
    $master->[0]->{'path_myddir'},
    $master->[1]->{'path_myddir'});

  if ($use_slaves)
  {
    push @data_dir_lst, ($slave->[0]->{'path_myddir'},
                         $slave->[1]->{'path_myddir'},
                         $slave->[2]->{'path_myddir'});
  }

  unless ($opt_skip_im)
  {
    foreach my $instance (@{$instance_manager->{'instances'}})
    {
      push(@data_dir_lst, $instance->{'path_datadir'});
    }
  }
}


##############################################################################
#
#  Set paths to various executable programs
#
##############################################################################

sub executable_setup () {

  #
  # Check if libtool is available in this distribution/clone
  # we need it when valgrinding or debugging non installed binary
  # Otherwise valgrind will valgrind the libtool wrapper or bash
  # and gdb will not find the real executable to debug
  #
  if ( -x "../libtool")
  {
    $exe_libtool= "../libtool";
    if ($opt_valgrind or $glob_debugger)
    {
      mtr_report("Using \"$exe_libtool\" when running valgrind or debugger");
    }
  }

  if ( $opt_source_dist )
  {
    if ( $glob_win32 )
    {
      $path_client_bindir= mtr_path_exists("$glob_basedir/client_release",
					   "$glob_basedir/client_debug",
                                           "$glob_basedir/bin",
                                           # New CMake locations.
                                           "$glob_basedir/client/release",
                                           "$glob_basedir/client/debug");
      $exe_mysqld=         mtr_exe_exists ("$path_client_bindir/mysqld-max-nt",
                                           "$path_client_bindir/mysqld-max",
                                           "$path_client_bindir/mysqld-nt",
                                           "$path_client_bindir/mysqld",
                                           "$path_client_bindir/mysqld-max",
                                           "$glob_basedir/sql/release/mysqld",
                                           "$glob_basedir/sql/debug/mysqld");
                                           "$path_client_bindir/mysqld-debug",
      $path_language=      mtr_path_exists("$glob_basedir/share/english/",
                                           "$glob_basedir/sql/share/english/");
      $path_charsetsdir=   mtr_path_exists("$glob_basedir/share/charsets",
                                           "$glob_basedir/sql/share/charsets");
      $exe_my_print_defaults=
        mtr_exe_exists("$path_client_bindir/my_print_defaults",
                       "$glob_basedir/extra/release/my_print_defaults",
                       "$glob_basedir/extra/debug/my_print_defaults");
      $exe_perror=
	mtr_exe_exists("$path_client_bindir/perror",
                       "$glob_basedir/extra/release/perror",
                       "$glob_basedir/extra/debug/perror");
    }
    else
    {
      $path_client_bindir= mtr_path_exists("$glob_basedir/client");
      $exe_mysqld=         mtr_exe_exists ("$glob_basedir/sql/mysqld");
      $exe_mysqlslap=      mtr_exe_exists ("$path_client_bindir/mysqlslap");
      $path_language=      mtr_path_exists("$glob_basedir/sql/share/english/");
      $path_charsetsdir=   mtr_path_exists("$glob_basedir/sql/share/charsets");

      $exe_im= mtr_exe_exists(
        "$glob_basedir/server-tools/instance-manager/mysqlmanager");
      $exe_my_print_defaults=
        mtr_exe_exists("$glob_basedir/extra/my_print_defaults");
      $exe_perror=
	mtr_exe_exists("$glob_basedir/extra/perror");
    }

    if ( $glob_use_embedded_server )
    {
      my $path_examples= "$glob_basedir/libmysqld/examples";
      $exe_mysqltest=    mtr_exe_exists("$path_examples/mysqltest_embedded");
      $exe_mysql_client_test=
        mtr_exe_exists("$path_examples/mysql_client_test_embedded",
		       "/usr/bin/false");
    }
    else
    {
      $exe_mysqltest= mtr_exe_exists("$path_client_bindir/mysqltest");
      $exe_mysql_client_test=
        mtr_exe_exists("$glob_basedir/tests/mysql_client_test",
                       "$glob_basedir/tests/release/mysql_client_test",
                       "$glob_basedir/tests/debug/mysql_client_test",
                       "$path_client_bindir/mysql_client_test",
		       "/usr/bin/false");
    }
    $exe_mysqlcheck=     mtr_exe_exists("$path_client_bindir/mysqlcheck");
    $exe_mysqldump=      mtr_exe_exists("$path_client_bindir/mysqldump");
    $exe_mysqlimport=    mtr_exe_exists("$path_client_bindir/mysqlimport");
    $exe_mysqlshow=      mtr_exe_exists("$path_client_bindir/mysqlshow");
    $exe_mysqlbinlog=    mtr_exe_exists("$path_client_bindir/mysqlbinlog");
    $exe_mysqladmin=     mtr_exe_exists("$path_client_bindir/mysqladmin");
    $exe_mysql=          mtr_exe_exists("$path_client_bindir/mysql");
    $exe_mysql_fix_system_tables=
      mtr_script_exists("$glob_basedir/scripts/mysql_fix_privilege_tables",
                        "/usr/bin/false");
    $path_ndb_tools_dir= mtr_path_exists("$glob_basedir/storage/ndb/tools");
    $exe_ndb_mgm=        "$glob_basedir/storage/ndb/src/mgmclient/ndb_mgm";
    $exe_ndb_waiter=     "$glob_basedir/storage/ndb/tools/ndb_waiter";
    $exe_ndbd=           "$glob_basedir/storage/ndb/src/kernel/ndbd";
    $exe_ndb_mgmd=       "$glob_basedir/storage/ndb/src/mgmsrv/ndb_mgmd";
    $lib_udf_example=
      mtr_file_exists("$glob_basedir/sql/.libs/udf_example.so");
  }
  else
  {
    $path_client_bindir= mtr_path_exists("$glob_basedir/bin");
    $exe_mysqlcheck=     mtr_exe_exists("$path_client_bindir/mysqlcheck");
    $exe_mysqldump=      mtr_exe_exists("$path_client_bindir/mysqldump");
    $exe_mysqlimport=    mtr_exe_exists("$path_client_bindir/mysqlimport");
    $exe_mysqlshow=      mtr_exe_exists("$path_client_bindir/mysqlshow");
    $exe_mysqlbinlog=    mtr_exe_exists("$path_client_bindir/mysqlbinlog");
    $exe_mysqladmin=     mtr_exe_exists("$path_client_bindir/mysqladmin");
    $exe_mysql=          mtr_exe_exists("$path_client_bindir/mysql");
    $exe_mysql_fix_system_tables=
      mtr_script_exists("$path_client_bindir/mysql_fix_privilege_tables",
			"$glob_basedir/scripts/mysql_fix_privilege_tables",
                        "/usr/bin/false");
    $exe_my_print_defaults=
      mtr_exe_exists("$path_client_bindir/my_print_defaults");
    $exe_perror=
      mtr_exe_exists("$path_client_bindir/perror");

    $path_language=      mtr_path_exists("$glob_basedir/share/mysql/english/",
                                         "$glob_basedir/share/english/");
    $path_charsetsdir=   mtr_path_exists("$glob_basedir/share/mysql/charsets",
                                         "$glob_basedir/share/charsets");

    if ( $glob_win32 )
    {
      $exe_mysqld=         mtr_exe_exists ("$glob_basedir/bin/mysqld-nt",
                                           "$glob_basedir/bin/mysqld",
                                           "$glob_basedir/bin/mysqld-debug",);
    }
    else
    {
      $exe_mysqld=         mtr_exe_exists ("$glob_basedir/libexec/mysqld",
                                           "$glob_basedir/bin/mysqld");
      $exe_mysqlslap=      mtr_exe_exists("$path_client_bindir/mysqlslap");
    }
    $exe_im= mtr_exe_exists("$glob_basedir/libexec/mysqlmanager",
                            "$glob_basedir/bin/mysqlmanager");
    if ( $glob_use_embedded_server )
    {
      $exe_mysqltest= mtr_exe_exists("$path_client_bindir/mysqltest_embedded");
      $exe_mysql_client_test=
        mtr_exe_exists("$glob_basedir/tests/mysql_client_test_embedded",
                       "$path_client_bindir/mysql_client_test_embedded",
		       "/usr/bin/false");
    }
    else
    {
      $exe_mysqltest= mtr_exe_exists("$path_client_bindir/mysqltest");
      $exe_mysql_client_test=
        mtr_exe_exists("$path_client_bindir/mysql_client_test",
                       "$glob_basedir/tests/release/mysql_client_test",
                       "$glob_basedir/tests/debug/mysql_client_test",
		       "/usr/bin/false"); # FIXME temporary
    }

    $path_ndb_tools_dir=  "$glob_basedir/bin";
    $exe_ndb_mgm=         "$glob_basedir/bin/ndb_mgm";
    $exe_ndb_waiter=      "$glob_basedir/bin/ndb_waiter";
    $exe_ndbd=            "$glob_basedir/bin/ndbd";
    $exe_ndb_mgmd=        "$glob_basedir/bin/ndb_mgmd";
  }

  $exe_master_mysqld= $exe_master_mysqld || $exe_mysqld;
  $exe_slave_mysqld=  $exe_slave_mysqld  || $exe_mysqld;

  $file_ndb_testrun_log= "$opt_vardir/log/ndb_testrun.log";
}


sub generate_cmdline_mysqldump ($) {
  my($mysqld) = @_;
  return
    "$exe_mysqldump --no-defaults -uroot " .
      "--port=$mysqld->{'port'} " .
      "--socket=$mysqld->{'path_sock'} --password=";
}


##############################################################################
#
#  Set environment to be used by childs of this process for
#  things that are constant duting the whole lifetime of mysql-test-run.pl
#
##############################################################################

# Note that some env is setup in spawn/run, in "mtr_process.pl"

sub environment_setup () {

  umask(022);

  my $extra_ld_library_paths;

  # --------------------------------------------------------------------------
  # Setup LD_LIBRARY_PATH so the libraries from this distro/clone
  # are used in favor of the system installed ones
  # --------------------------------------------------------------------------
  if ( $opt_source_dist )
  {
    $extra_ld_library_paths= "$glob_basedir/libmysql/.libs/";
  }
  else
  {
    $extra_ld_library_paths= "$glob_basedir/lib";
  }

  # --------------------------------------------------------------------------
  # Add the path where mysqld will find udf_example.so
  # --------------------------------------------------------------------------
  $extra_ld_library_paths .= ":" .
    ($lib_udf_example ?  dirname($lib_udf_example) : "");

  $ENV{'LD_LIBRARY_PATH'}=
    "$extra_ld_library_paths" .
      ($ENV{'LD_LIBRARY_PATH'} ? ":$ENV{'LD_LIBRARY_PATH'}" : "");
  $ENV{'DYLD_LIBRARY_PATH'}=
    "$extra_ld_library_paths" .
      ($ENV{'DYLD_LIBRARY_PATH'} ? ":$ENV{'DYLD_LIBRARY_PATH'}" : "");

  # --------------------------------------------------------------------------
  # Also command lines in .opt files may contain env vars
  # --------------------------------------------------------------------------

  $ENV{'CHARSETSDIR'}=              $path_charsetsdir;
  $ENV{'UMASK'}=              "0660"; # The octal *string*
  $ENV{'UMASK_DIR'}=          "0770"; # The octal *string*
  $ENV{'LC_COLLATE'}=         "C";
  $ENV{'USE_RUNNING_SERVER'}= $glob_use_running_server;
  $ENV{'MYSQL_TEST_DIR'}=     $glob_mysql_test_dir;
  $ENV{'MYSQLTEST_VARDIR'}=   $opt_vardir;
  $ENV{'MYSQL_TMP_DIR'}=      $opt_tmpdir;
  $ENV{'MASTER_MYSOCK'}=      $master->[0]->{'path_sock'};
  $ENV{'MASTER_MYSOCK1'}=     $master->[1]->{'path_sock'};
  $ENV{'MASTER_MYPORT'}=      $master->[0]->{'port'};
  $ENV{'MASTER_MYPORT1'}=     $master->[1]->{'port'};
  $ENV{'SLAVE_MYPORT'}=       $slave->[0]->{'port'};
  $ENV{'SLAVE_MYPORT1'}=      $slave->[1]->{'port'};
  $ENV{'SLAVE_MYPORT2'}=      $slave->[2]->{'port'};
# $ENV{'MYSQL_TCP_PORT'}=     '@MYSQL_TCP_PORT@'; # FIXME
  $ENV{'MYSQL_TCP_PORT'}=     3306;

  $ENV{MTR_BUILD_THREAD}= 0 unless $ENV{MTR_BUILD_THREAD}; # Set if not set

  # ----------------------------------------------------
  # Setup env for NDB
  # ----------------------------------------------------
  $ENV{'NDB_MGM'}=                  $exe_ndb_mgm;

  $ENV{'NDBCLUSTER_PORT'}=          $opt_ndbcluster_port;
  $ENV{'NDBCLUSTER_PORT_SLAVE'}=    $opt_ndbcluster_port_slave;

  $ENV{'NDB_EXTRA_TEST'}=           $opt_ndb_extra_test;

  $ENV{'NDB_BACKUP_DIR'}=           $clusters->[0]->{'data_dir'};
  $ENV{'NDB_DATA_DIR'}=             $clusters->[0]->{'data_dir'};
  $ENV{'NDB_TOOLS_DIR'}=            $path_ndb_tools_dir;
  $ENV{'NDB_TOOLS_OUTPUT'}=         $file_ndb_testrun_log;
  $ENV{'NDB_CONNECTSTRING'}=        $opt_ndbconnectstring;


  # ----------------------------------------------------
  # Setup env for IM
  # ----------------------------------------------------
  $ENV{'IM_EXE'}=             $exe_im;
  $ENV{'IM_PATH_PID'}=        $instance_manager->{path_pid};
  $ENV{'IM_PATH_ANGEL_PID'}=  $instance_manager->{path_angel_pid};
  $ENV{'IM_PORT'}=            $instance_manager->{port};
  $ENV{'IM_DEFAULTS_PATH'}=   $instance_manager->{defaults_file};
  $ENV{'IM_PASSWORD_PATH'}=   $instance_manager->{password_file};

  $ENV{'IM_MYSQLD1_SOCK'}=    $instance_manager->{instances}->[0]->{path_sock};
  $ENV{'IM_MYSQLD1_PORT'}=    $instance_manager->{instances}->[0]->{port};
  $ENV{'IM_MYSQLD1_PATH_PID'}=$instance_manager->{instances}->[0]->{path_pid};
  $ENV{'IM_MYSQLD2_SOCK'}=    $instance_manager->{instances}->[1]->{path_sock};
  $ENV{'IM_MYSQLD2_PORT'}=    $instance_manager->{instances}->[1]->{port};
  $ENV{'IM_MYSQLD2_PATH_PID'}=$instance_manager->{instances}->[1]->{path_pid};

  # ----------------------------------------------------
  # Setup env so childs can execute mysqlcheck
  # ----------------------------------------------------
  my $cmdline_mysqlcheck=
    "$exe_mysqlcheck --no-defaults -uroot " .
    "--port=$master->[0]->{'port'} " .
    "--socket=$master->[0]->{'path_sock'} --password=";

  if ( $opt_debug )
  {
    $cmdline_mysqlcheck .=
      " --debug=d:t:A,$opt_vardir_trace/log/mysqlcheck.trace";
  }
  $ENV{'MYSQL_CHECK'}=              $cmdline_mysqlcheck;

  # ----------------------------------------------------
  # Setup env to childs can execute myqldump
  # ----------------------------------------------------
  my $cmdline_mysqldump= generate_cmdline_mysqldump($master->[0]);
  my $cmdline_mysqldumpslave= generate_cmdline_mysqldump($slave->[0]);

  if ( $opt_debug )
  {
    $cmdline_mysqldump .=
      " --debug=d:t:A,$opt_vardir_trace/log/mysqldump-master.trace";
    $cmdline_mysqldumpslave .=
      " --debug=d:t:A,$opt_vardir_trace/log/mysqldump-slave.trace";
  }
  $ENV{'MYSQL_DUMP'}= $cmdline_mysqldump;
  $ENV{'MYSQL_DUMP_SLAVE'}= $cmdline_mysqldumpslave;


  # ----------------------------------------------------
  # Setup env so childs can execute mysqlslap
  # ----------------------------------------------------
  unless ( $glob_win32 )
  {
    my $cmdline_mysqlslap=
      "$exe_mysqlslap -uroot " .
      "--port=$master->[0]->{'port'} " .
      "--socket=$master->[0]->{'path_sock'} --password= " .
      "--lock-directory=$opt_tmpdir";

    if ( $opt_debug )
    {
      $cmdline_mysqlslap .=
        " --debug=d:t:A,$opt_vardir_trace/log/mysqlslap.trace";
    }
    $ENV{'MYSQL_SLAP'}= $cmdline_mysqlslap;
  }

  # ----------------------------------------------------
  # Setup env so childs can execute mysqlimport
  # ----------------------------------------------------
  my $cmdline_mysqlimport=
    "$exe_mysqlimport -uroot " .
    "--port=$master->[0]->{'port'} " .
    "--socket=$master->[0]->{'path_sock'} --password=";

  if ( $opt_debug )
  {
    $cmdline_mysqlimport .=
      " --debug=d:t:A,$opt_vardir_trace/log/mysqlimport.trace";
  }
  $ENV{'MYSQL_IMPORT'}= $cmdline_mysqlimport;


  # ----------------------------------------------------
  # Setup env so childs can execute mysqlshow
  # ----------------------------------------------------
  my $cmdline_mysqlshow=
    "$exe_mysqlshow -uroot " .
    "--port=$master->[0]->{'port'} " .
    "--socket=$master->[0]->{'path_sock'} --password=";

  if ( $opt_debug )
  {
    $cmdline_mysqlshow .=
      " --debug=d:t:A,$opt_vardir_trace/log/mysqlshow.trace";
  }
  $ENV{'MYSQL_SHOW'}= $cmdline_mysqlshow;

  # ----------------------------------------------------
  # Setup env so childs can execute mysqlbinlog
  # ----------------------------------------------------
  my $cmdline_mysqlbinlog=
    "$exe_mysqlbinlog" .
      " --no-defaults --local-load=$opt_tmpdir" .
      " --character-sets-dir=$path_charsetsdir";

  if ( $opt_debug )
  {
    $cmdline_mysqlbinlog .=
      " --debug=d:t:A,$opt_vardir_trace/log/mysqlbinlog.trace";
  }
  $ENV{'MYSQL_BINLOG'}= $cmdline_mysqlbinlog;

  # ----------------------------------------------------
  # Setup env so childs can execute mysql
  # ----------------------------------------------------
  my $cmdline_mysql=
    "$exe_mysql --no-defaults --host=localhost  --user=root --password= " .
    "--port=$master->[0]->{'port'} " .
    "--socket=$master->[0]->{'path_sock'}";

  $ENV{'MYSQL'}= $cmdline_mysql;

  # ----------------------------------------------------
  # Setup env so childs can execute mysql_client_test
  # ----------------------------------------------------
  my $cmdline_mysql_client_test=
    "$exe_mysql_client_test --no-defaults --testcase --user=root --silent " .
    "--port=$master->[0]->{'port'} " .
    "--vardir=$opt_vardir " .
    "--socket=$master->[0]->{'path_sock'}";

  if ( $opt_debug )
  {
    $cmdline_mysql_client_test .=
      " --debug=d:t:A,$opt_vardir_trace/log/mysql_client_test.trace";
  }

  if ( $glob_use_embedded_server )
  {
    $cmdline_mysql_client_test.=
      " -A --language=$path_language" .
      " -A --datadir=$slave->[0]->{'path_myddir'}" .
      " -A --character-sets-dir=$path_charsetsdir";
  }
  $ENV{'MYSQL_CLIENT_TEST'}= $cmdline_mysql_client_test;


  # ----------------------------------------------------
  # Setup env so childs can execute mysql_fix_system_tables
  # ----------------------------------------------------
  my $cmdline_mysql_fix_system_tables=
    "$exe_mysql_fix_system_tables --no-defaults --host=localhost --user=root --password= " .
    "--basedir=$glob_basedir --bindir=$path_client_bindir --verbose " .
    "--port=$master->[0]->{'port'} " .
    "--socket=$master->[0]->{'path_sock'}";

  $ENV{'MYSQL_FIX_SYSTEM_TABLES'}=  $cmdline_mysql_fix_system_tables;

  # ----------------------------------------------------
  # Setup env so childs can execute my_print_defaults
  # ----------------------------------------------------
  $ENV{'MYSQL_MY_PRINT_DEFAULTS'}=  $exe_my_print_defaults;


  # ----------------------------------------------------
  # Setup env so childs can execute perror  
  # ----------------------------------------------------
  $ENV{'MY_PERROR'}=                 $exe_perror;

  # ----------------------------------------------------
  # Add the path where mysqld will find udf_example.so
  # ----------------------------------------------------
  $ENV{'UDF_EXAMPLE_LIB'}=
    ($lib_udf_example ? basename($lib_udf_example) : "");

  $ENV{'LD_LIBRARY_PATH'}=
    ($lib_udf_example ?  dirname($lib_udf_example) : "") .
      ($ENV{'LD_LIBRARY_PATH'} ? ":$ENV{'LD_LIBRARY_PATH'}" : "");


  # ----------------------------------------------------
  # We are nice and report a bit about our settings
  # ----------------------------------------------------
  if (!$opt_extern)
  {
    print "Using MTR_BUILD_THREAD      = $ENV{MTR_BUILD_THREAD}\n";
    print "Using MASTER_MYPORT         = $ENV{MASTER_MYPORT}\n";
    print "Using MASTER_MYPORT1        = $ENV{MASTER_MYPORT1}\n";
    print "Using SLAVE_MYPORT          = $ENV{SLAVE_MYPORT}\n";
    print "Using SLAVE_MYPORT1         = $ENV{SLAVE_MYPORT1}\n";
    print "Using SLAVE_MYPORT2         = $ENV{SLAVE_MYPORT2}\n";
    print "Using NDBCLUSTER_PORT       = $ENV{NDBCLUSTER_PORT}\n";
    print "Using NDBCLUSTER_PORT_SLAVE = $ENV{NDBCLUSTER_PORT_SLAVE}\n";
    print "Using IM_PORT               = $ENV{IM_PORT}\n";
    print "Using IM_MYSQLD1_PORT       = $ENV{IM_MYSQLD1_PORT}\n";
    print "Using IM_MYSQLD2_PORT       = $ENV{IM_MYSQLD2_PORT}\n";
  }
}


##############################################################################
#
#  If we get a ^C, we try to clean up before termination
#
##############################################################################
# FIXME check restrictions what to do in a signal handler

sub signal_setup () {
  $SIG{INT}= \&handle_int_signal;
}


sub handle_int_signal () {
  $SIG{INT}= 'DEFAULT';         # If we get a ^C again, we die...
  mtr_warning("got INT signal, cleaning up.....");
  stop_all_servers();
  mtr_error("We die from ^C signal from user");
}


##############################################################################
#
#  Handle left overs from previous runs
#
##############################################################################

sub kill_running_server () {

  if ( $opt_fast or $glob_use_embedded_server )
  {
    # FIXME is embedded server really using PID files?!
    unlink($master->[0]->{'path_pid'});
    unlink($master->[1]->{'path_pid'});
    unlink($slave->[0]->{'path_pid'});
    unlink($slave->[1]->{'path_pid'});
    unlink($slave->[2]->{'path_pid'});
  }
  else
  {
    # Ensure that no old mysqld test servers are running
    # This is different from terminating processes we have
    # started from ths run of the script, this is terminating
    # leftovers from previous runs.

    mtr_report("Killing Possible Leftover Processes");
    mkpath("$opt_vardir/log"); # Needed for mysqladmin log

    mtr_kill_leftovers();

   }
}

sub cleanup_stale_files () {

  mtr_report("Removing Stale Files");

  if ( $opt_vardir eq "$glob_mysql_test_dir/var" )
  {
    #
    # Running with "var" in mysql-test dir
    #
    if ( -l "$glob_mysql_test_dir/var" )
    {
      # Some users creates a soft link in mysql-test/var to another area
      # - allow it
      mtr_report("WARNING: Using the 'mysql-test/var' symlink");
      rmtree("$opt_vardir/log");
      rmtree("$opt_vardir/ndbcluster-$opt_ndbcluster_port");
      rmtree("$opt_vardir/run");
      rmtree("$opt_vardir/tmp");
    }
    else
    {
      # Remove the entire "var" dir
      rmtree("$opt_vardir/");
    }
  }
  else
  {
    #
    # Running with "var" in some other place
    #

    # Remove the var/ dir in mysql-test dir if any
    # this could be an old symlink that shouldn't be there
    rmtree("$glob_mysql_test_dir/var");

    # Remove the "var" dir
    rmtree("$opt_vardir/");
  }

  mkpath("$opt_vardir/log");
  mkpath("$opt_vardir/run");
  mkpath("$opt_vardir/tmp");
  mkpath($opt_tmpdir) if $opt_tmpdir ne "$opt_vardir/tmp";

  # Remove old and create new data dirs
  foreach my $data_dir (@data_dir_lst)
  {
    rmtree("$data_dir");
    mkpath("$data_dir/mysql");
    mkpath("$data_dir/test");
  }

  # Make a link std_data_ln in var/ that points to std_data
  if ( ! $glob_win32 )
  {
    symlink("$glob_mysql_test_dir/std_data", "$opt_vardir/std_data_ln");
  }
  else
  {
    # on windows, copy all files from std_data into var/std_data_ln
    mkpath("$opt_vardir/std_data_ln");
    opendir(DIR, "$glob_mysql_test_dir/std_data")
      or mtr_error("Can't find the std_data directory: $!");
    for(readdir(DIR)) {
      next if -d "$glob_mysql_test_dir/std_data/$_";
      copy("$glob_mysql_test_dir/std_data/$_", "$opt_vardir/std_data_ln/$_");
    }
    closedir(DIR);
  }
}


sub  check_running_as_root () {
  # Check if running as root
  # i.e a file can be read regardless what mode we set it to
  my $test_file= "$opt_vardir/test_running_as_root.txt";
  mtr_tofile($test_file, "MySQL");
  chmod(oct("0000"), $test_file);

  my $result="";
  if (open(FILE,"<",$test_file))
  {
    $result= join('', <FILE>);
    close FILE;
  }

  chmod(oct("0755"), $test_file);
  unlink($test_file);

  $ENV{'MYSQL_TEST_ROOT'}= "NO";
  if ($result eq "MySQL")
  {
    mtr_warning("running this script as _root_ will cause some " .
                "tests to be skipped");
    $ENV{'MYSQL_TEST_ROOT'}= "YES";
  }
}



sub check_ssl_support () {

  if ($opt_skip_ssl || $opt_extern)
  {
    mtr_report("Skipping SSL");
    $opt_ssl_supported= 0;
    $opt_ssl= 0;
    return;
  }

  # check ssl support by testing using a switch
  # that is only available in that case
  if ( mtr_run($exe_mysqld,
	       ["--no-defaults",
	        "--ssl",
	        "--help"],
	       "", "/dev/null", "/dev/null", "") != 0 )
  {
    if ( $opt_ssl)
    {
      mtr_error("Couldn't find support for SSL");
      return;
    }
    mtr_report("Skipping SSL, mysqld not compiled with SSL");
    $opt_ssl_supported= 0;
    $opt_ssl= 0;
    return;
  }
  mtr_report("Setting mysqld to support SSL connections");
  $opt_ssl_supported= 1;
}


sub check_debug_support () {

  # check debug support by testing using a switch
  # that is only available in that case
  if ( mtr_run($exe_mysqld,
	       ["--no-defaults",
	        "--debug",
	        "--help"],
	       "", "/dev/null", "/dev/null", "") != 0 )
  {
    # mtr_report("Binaries are not debug compiled");
    $debug_compiled_binaries= 0;

    if ( $opt_debug )
    {
      mtr_error("Can't use --debug, binaries does not support it");
    }
    return;
  }
  mtr_report("Binaries are debug compiled");
  $debug_compiled_binaries= 1;
}

##############################################################################
#
#  Start the ndb cluster
#
##############################################################################

sub check_ndbcluster_support () {

  if ($opt_skip_ndbcluster)
  {
    mtr_report("Skipping ndbcluster");
    $opt_skip_ndbcluster_slave= 1;
    return;
  }

  # check ndbcluster support by runnning mysqld using a switch
  # that is only available in that case
  if ( mtr_run($exe_mysqld,
	       ["--no-defaults",
	        "--ndb-use-exact-count",
	        "--help"],
	       "", "/dev/null", "/dev/null", "") != 0 )
  {
    mtr_report("Skipping ndbcluster, mysqld not compiled with ndbcluster");
    $opt_skip_ndbcluster= 1;
    $opt_skip_ndbcluster_slave= 1;
    return;
  }
  $opt_ndbcluster_supported= 1;
  mtr_report("Using ndbcluster when necessary, mysqld supports it");
  return;
}


sub ndbcluster_start_install ($) {
  my $cluster= shift;

  if ( $opt_skip_ndbcluster or $glob_use_running_ndbcluster )
  {
    return 0;
  }

  mtr_report("Installing $cluster->{'name'} Cluster");

  mkdir($cluster->{'data_dir'});

  # Create a config file from template
  my $ndb_no_ord=512;
  my $ndb_no_attr=2048;
  my $ndb_con_op=105000;
  my $ndb_dmem="80M";
  my $ndb_imem="24M";
  my $ndb_pbmem="32M";
  my $nodes= $cluster->{'nodes'};
  my $ndb_host= "localhost";
  my $ndb_diskless= 0;

  if (!$opt_bench)
  {
    # Use a smaller configuration
    $ndb_no_ord=32;
    $ndb_con_op=5000;
    $ndb_dmem="20M";
    $ndb_imem="1M";
    $ndb_pbmem="4M";
  }

  my $config_file_template=     "ndb/ndb_config_${nodes}_node.ini";
  my $config_file= "$cluster->{'data_dir'}/config.ini";

  open(IN, $config_file_template)
    or mtr_error("Can't open $config_file_template: $!");
  open(OUT, ">", $config_file)
    or mtr_error("Can't write to $config_file: $!");
  while (<IN>)
  {
    chomp;

    s/CHOOSE_MaxNoOfAttributes/$ndb_no_attr/;
    s/CHOOSE_MaxNoOfOrderedIndexes/$ndb_no_ord/;
    s/CHOOSE_MaxNoOfConcurrentOperations/$ndb_con_op/;
    s/CHOOSE_DataMemory/$ndb_dmem/;
    s/CHOOSE_IndexMemory/$ndb_imem/;
    s/CHOOSE_Diskless/$ndb_diskless/;
    s/CHOOSE_HOSTNAME_.*/$ndb_host/;
    s/CHOOSE_FILESYSTEM/$cluster->{'data_dir'}/;
    s/CHOOSE_PORT_MGM/$cluster->{'port'}/;
    s/CHOOSE_DiskPageBufferMemory/$ndb_pbmem/;

    print OUT "$_ \n";
  }
  close OUT;
  close IN;


  # Start cluster with "--initial"

  ndbcluster_start($cluster, "--initial");

  return 0;
}


sub ndbcluster_wait_started($$){
  my $cluster= shift;
  my $ndb_waiter_extra_opt= shift;
  my $path_waiter_log= "$cluster->{'data_dir'}/ndb_waiter.log";
  my $args;

  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--core");
  mtr_add_arg($args, "--ndb-connectstring=%s", $cluster->{'connect_string'});
  mtr_add_arg($args, "--timeout=60");

  if ($ndb_waiter_extra_opt)
  {
    mtr_add_arg($args, "$ndb_waiter_extra_opt");
  }

  # Start the ndb_waiter which will connect to the ndb_mgmd
  # and poll it for state of the ndbd's, will return when
  # all nodes in the cluster is started
  my $res= mtr_run($exe_ndb_waiter, $args,
		   "", $path_waiter_log, $path_waiter_log, "");
  mtr_verbose("ndbcluster_wait_started, returns: $res") if $res;
  return $res;
}



sub mysqld_wait_started($){
  my $mysqld= shift;

  my $res= sleep_until_file_created($mysqld->{'path_pid'},
				    $mysqld->{'start_timeout'},
				    $mysqld->{'pid'});
  return $res == 0;
}


sub ndb_mgmd_wait_started($) {
  my ($cluster)= @_;

  my $retries= 100;
  while (ndbcluster_wait_started($cluster, "--no-contact") and
	 $retries)
  {
    # Millisceond sleep emulated with select
    select(undef, undef, undef, (0.1));

    $retries--;
  }

  return $retries == 0;

}

sub ndb_mgmd_start ($) {
  my $cluster= shift;

  my $args;                             # Arg vector
  my $pid= -1;

  mtr_init_args(\$args);
  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--core");
  mtr_add_arg($args, "--nodaemon");
  mtr_add_arg($args, "--config-file=%s", "$cluster->{'data_dir'}/config.ini");


  my $path_ndb_mgmd_log= "$cluster->{'data_dir'}/\l$cluster->{'name'}_ndb_mgmd.log";
  $pid= mtr_spawn($exe_ndb_mgmd, $args, "",
		  $path_ndb_mgmd_log,
		  $path_ndb_mgmd_log,
		  "",
		  { append_log_file => 1 });

  # FIXME Should not be needed
  # Unfortunately the cluster nodes will fail to start
  # if ndb_mgmd has not started properly
  if (ndb_mgmd_wait_started($cluster))
  {
    mtr_error("Failed to wait for start of ndb_mgmd");
  }

  # Remember pid of ndb_mgmd
  $cluster->{'pid'}= $pid;

  mtr_verbose("ndb_mgmd_start, pid: $pid");

  return $pid;
}


sub ndbd_start ($$$) {
  my $cluster= shift;
  my $idx= shift;
  my $extra_args= shift;

  my $args;                             # Arg vector
  my $pid= -1;

  mtr_init_args(\$args);
  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--core");
  mtr_add_arg($args, "--ndb-connectstring=%s", "$cluster->{'connect_string'}");
  mtr_add_arg($args, "--character-sets-dir=%s", "$path_charsetsdir");
  mtr_add_arg($args, "--nodaemon");
  mtr_add_arg($args, "$extra_args");

  my $nodeid= $cluster->{'ndbds'}->[$idx]->{'nodeid'};
  my $path_ndbd_log= "$cluster->{'data_dir'}/ndb_${nodeid}.log";
  $pid= mtr_spawn($exe_ndbd, $args, "",
		  $path_ndbd_log,
		  $path_ndbd_log,
		  "",
		  { append_log_file => 1 });

  # Add pid to list of pids for this cluster
  $cluster->{'ndbds'}->[$idx]->{'pid'}= $pid;

  # Rememeber options used when starting
  $cluster->{'ndbds'}->[$idx]->{'start_extra_args'}= $extra_args;
  $cluster->{'ndbds'}->[$idx]->{'idx'}= $idx;

  mtr_verbose("ndbd_start, pid: $pid");

  return $pid;
}


sub ndbcluster_start ($$) {
  my $cluster= shift;
  my $extra_args= shift;

  mtr_verbose("ndbcluster_start '$cluster->{'name'}'");

  if ( $glob_use_running_ndbcluster )
  {
    return 0;
  }

  if ( $cluster->{'pid'} )
  {
    mtr_error("Cluster '$cluster->{'name'}' already started");
  }

  ndb_mgmd_start($cluster);

  for ( my $idx= 0; $idx < $cluster->{'nodes'}; $idx++ )
  {
    ndbd_start($cluster, $idx, $extra_args);
  }

  return 0;
}


sub rm_ndbcluster_tables ($) {
  my $dir=       shift;
  foreach my $bin ( glob("$dir/cluster/apply_status*"),
                    glob("$dir/cluster/schema*") )
  {
    unlink($bin);
  }
}


##############################################################################
#
#  Run the benchmark suite
#
##############################################################################

sub run_benchmarks ($) {
  my $benchmark=  shift;

  my $args;

  if ( ! $glob_use_embedded_server )
  {
    mysqld_start($master->[0],[],[]);
    if ( ! $master->[0]->{'pid'} )
    {
      mtr_error("Can't start the mysqld server");
    }
  }

  mtr_init_args(\$args);

  mtr_add_arg($args, "--socket=%s", $master->[0]->{'path_sock'});
  mtr_add_arg($args, "--user=%s", $opt_user);

  if ( $opt_small_bench )
  {
    mtr_add_arg($args, "--small-test");
    mtr_add_arg($args, "--small-tables");
  }

  if ( $opt_with_ndbcluster )
  {
    mtr_add_arg($args, "--create-options=TYPE=ndb");
  }

  chdir($glob_mysql_bench_dir)
    or mtr_error("Couldn't chdir to '$glob_mysql_bench_dir': $!");

  # FIXME write shorter....

  if ( ! $benchmark )
  {
    mtr_add_arg($args, "--log");
    mtr_run("$glob_mysql_bench_dir/run-all-tests", $args, "", "", "", "");
    # FIXME check result code?!
  }
  elsif ( -x $benchmark )
  {
    mtr_run("$glob_mysql_bench_dir/$benchmark", $args, "", "", "", "");
    # FIXME check result code?!
  }
  else
  {
    mtr_error("Benchmark $benchmark not found");
  }

  chdir($glob_mysql_test_dir);          # Go back

  if ( ! $glob_use_embedded_server )
  {
    stop_masters();
  }
}


##############################################################################
#
#  Run the test suite
#
##############################################################################

# FIXME how to specify several suites to run? Comma separated list?


sub run_suite () {
  my ($suite, $tests)= @_;

  mtr_print_thick_line();

  mtr_timer_start($glob_timers,"suite", 60 * $opt_suite_timeout);

  mtr_report("Starting Tests in the '$suite' suite");

  mtr_report_tests_not_skipped_though_disabled($tests);

  mtr_print_header();

  foreach my $tinfo ( @$tests )
  {
    if (run_testcase_check_skip_test($tinfo))
    {
      next;
    }

    mtr_timer_start($glob_timers,"testcase", 60 * $opt_testcase_timeout);
    run_testcase($tinfo);
    mtr_timer_stop($glob_timers,"testcase");
  }

  mtr_print_line();

  if ( ! $glob_debugger and
       ! $glob_use_running_server and
       ! $glob_use_embedded_server )
  {
    stop_all_servers();
  }

  if ( $opt_gcov )
  {
    gcov_collect(); # collect coverage information
  }
  if ( $opt_gprof )
  {
    gprof_collect(); # collect coverage information
  }

  mtr_report_stats($tests);

  mtr_timer_stop($glob_timers,"suite");
}


##############################################################################
#
#  Initiate the test databases
#
##############################################################################

sub initialize_servers () {
  if ( ! $glob_use_running_server )
  {
    kill_running_server();

    unless ( $opt_start_dirty )
    {
      cleanup_stale_files();
      mysql_install_db();
      if ( $opt_force )
      {
	save_installed_db();
      }
    }
    check_running_as_root();
  }
}

sub mysql_install_db () {

  install_db('master', $master->[0]->{'path_myddir'});

  # FIXME check if testcase really is using second master
  copy_install_db('master', $master->[1]->{'path_myddir'});

  if ( $use_slaves )
  {
    install_db('slave',  $slave->[0]->{'path_myddir'});
    install_db('slave',  $slave->[1]->{'path_myddir'});
    install_db('slave',  $slave->[2]->{'path_myddir'});
  }

  if ( ! $opt_skip_im )
  {
    im_prepare_env($instance_manager);
  }

  my $cluster_started_ok= 1; # Assume it can be started

  if (ndbcluster_start_install($clusters->[0]) ||
      $use_slaves && ndbcluster_start_install($clusters->[1]))
  {
    mtr_warning("Failed to start install of cluster");
    $cluster_started_ok= 0;
  }

  foreach my $cluster (@{$clusters})
  {

    next if !$cluster->{'pid'};

    $cluster->{'installed_ok'}= "YES"; # Assume install suceeds

    if (ndbcluster_wait_started($cluster, ""))
    {
      # failed to install, disable usage and flag that its no ok
      mtr_report("ndbcluster_install of $cluster->{'name'} failed");
      $cluster->{"installed_ok"}= "NO";

      $cluster_started_ok= 0;
    }
  }

  $ENV{'NDB_STATUS_OK'}=            $clusters->[0]->{'installed_ok'};
  $ENV{'NDB_SLAVE_STATUS_OK'}=      $clusters->[1]->{'installed_ok'};;

  if ( ! $cluster_started_ok )
  {
    if ( $opt_force)
    {
      # Continue without cluster
    }
    else
    {
      mtr_error("To continue, re-run with '--force'.");
    }
  }

  # Stop clusters...
  stop_all_servers();

  return 0;
}


sub copy_install_db ($$) {
  my $type=      shift;
  my $data_dir=  shift;

  mtr_report("Installing \u$type Database");

  # Just copy the installed db from first master
  mtr_copy_dir($master->[0]->{'path_myddir'}, $data_dir);

}


sub install_db ($$) {
  my $type=      shift;
  my $data_dir=  shift;

  my $init_db_sql=     "lib/init_db.sql";
  my $init_db_sql_tmp= "/tmp/init_db.sql$$";
  my $args;

  mtr_report("Installing \u$type Database");

  open(IN, $init_db_sql)
    or mtr_error("Can't open $init_db_sql: $!");
  open(OUT, ">", $init_db_sql_tmp)
    or mtr_error("Can't write to $init_db_sql_tmp: $!");
  while (<IN>)
  {
    chomp;
    s/\@HOSTNAME\@/$glob_hostname/;
    if ( /^\s*$/ )
    {
      print OUT "\n";
    }
    elsif (/;$/)
    {
      print OUT "$_\n";
    }
    else
    {
      print OUT "$_ ";
    }
  }
  close OUT;
  close IN;

  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--bootstrap");
  mtr_add_arg($args, "--console");
  mtr_add_arg($args, "--skip-grant-tables");
  mtr_add_arg($args, "--basedir=%s", $path_my_basedir);
  mtr_add_arg($args, "--datadir=%s", $data_dir);
  mtr_add_arg($args, "--skip-innodb");
  mtr_add_arg($args, "--skip-ndbcluster");
  mtr_add_arg($args, "--tmpdir=.");

  if ( ! $opt_netware )
  {
    mtr_add_arg($args, "--language=%s", $path_language);
    mtr_add_arg($args, "--character-sets-dir=%s", $path_charsetsdir);
  }

  # Log bootstrap command
  my $path_bootstrap_log= "$opt_vardir/log/bootstrap.log";
  mtr_tofile($path_bootstrap_log,
	     "$exe_mysqld " . join(" ", @$args) . "\n");

  if ( mtr_run($exe_mysqld, $args, $init_db_sql_tmp,
               $path_bootstrap_log, $path_bootstrap_log,
	       "", { append_log_file => 1 }) != 0 )

  {
    unlink($init_db_sql_tmp);
    mtr_error("Error executing mysqld --bootstrap\n" .
              "Could not install $type test DBs");
  }
  unlink($init_db_sql_tmp);
}


sub im_prepare_env($) {
  my $instance_manager = shift;

  im_create_passwd_file($instance_manager);
  im_prepare_data_dir($instance_manager);
}


sub im_create_passwd_file($) {
  my $instance_manager = shift;

  my $pwd_file_path = $instance_manager->{'password_file'};

  mtr_report("Creating IM password file ($pwd_file_path)");

  open(OUT, ">", $pwd_file_path)
    or mtr_error("Can't write to $pwd_file_path: $!");

  print OUT $instance_manager->{'admin_login'}, ":",
        $instance_manager->{'admin_sha1'}, "\n";

  close(OUT);
}


sub im_create_defaults_file($) {
  my $instance_manager = shift;

  my $defaults_file = $instance_manager->{'defaults_file'};

  open(OUT, ">", $defaults_file)
    or mtr_error("Can't write to $defaults_file: $!");

  print OUT <<EOF
[mysql]

[manager]
pid-file            = $instance_manager->{path_pid}
angel-pid-file      = $instance_manager->{path_angel_pid}
socket              = $instance_manager->{path_sock}
port                = $instance_manager->{port}
password-file       = $instance_manager->{password_file}
default-mysqld-path = $exe_mysqld

EOF
;

  foreach my $instance (@{$instance_manager->{'instances'}})
  {
    my $server_id = $instance->{'server_id'};

    print OUT <<EOF
[mysqld$server_id]
socket              = $instance->{path_sock}
pid-file            = $instance->{path_pid}
port                = $instance->{port}
datadir             = $instance->{path_datadir}
log                 = $instance->{path_datadir}/mysqld$server_id.log
log-error           = $instance->{path_datadir}/mysqld$server_id.err.log
log-slow-queries    = $instance->{path_datadir}/mysqld$server_id.slow.log
language            = $path_language
character-sets-dir  = $path_charsetsdir
basedir             = $path_my_basedir
server_id           = $server_id
skip-stack-trace
skip-innodb
skip-ndbcluster
EOF
;

    print OUT "nonguarded\n" if $instance->{'nonguarded'};
    print OUT "log-output=FILE\n" if $instance->{'old_log_format'};
    print OUT "\n";
  }

  close(OUT);
}


sub im_prepare_data_dir($) {
  my $instance_manager = shift;

  foreach my $instance (@{$instance_manager->{'instances'}})
  {
    copy_install_db(
      'im_mysqld_' . $instance->{'server_id'},
      $instance->{'path_datadir'});
  }
}

sub run_testcase_check_skip_test($)
{
  my ($tinfo)= @_;

  # ----------------------------------------------------------------------
  # If marked to skip, just print out and return.
  # Note that a test case not marked as 'skip' can still be
  # skipped later, because of the test case itself in cooperation
  # with the mysqltest program tells us so.
  # ----------------------------------------------------------------------

  if ( $tinfo->{'skip'} )
  {
    mtr_report_test_name($tinfo);
    mtr_report_test_skipped($tinfo);
    return 1;
  }

  # If test needs cluster, check that master installed ok
  if ( $tinfo->{'ndb_test'}  and $clusters->[0]->{'installed_ok'} eq "NO" )
  {
    mtr_report_test_name($tinfo);
    mtr_report_test_failed($tinfo);
    return 1;
  }

  # If test needs slave cluster, check that it installed ok
  if ( $tinfo->{'ndb_test'}  and $tinfo->{'slave_num'} and
       $clusters->[1]->{'installed_ok'} eq "NO" )
  {
    mtr_report_test_name($tinfo);
    mtr_report_test_failed($tinfo);
    return 1;
  }

  return 0;
}


sub do_before_run_mysqltest($)
{
  my $tinfo= shift;
  my $tname= $tinfo->{'name'};

  # Remove old reject file
  if ( $opt_suite eq "main" )
  {
    unlink("r/$tname.reject");
  }
  else
  {
    unlink("suite/$opt_suite/r/$tname.reject");
  }


# MASV cleanup...
  mtr_tonewfile($path_current_test_log,"$tname\n"); # Always tell where we are

  # output current test to ndbcluster log file to enable diagnostics
  mtr_tofile($file_ndb_testrun_log,"CURRENT TEST $tname\n");

  mtr_tofile($master->[0]->{'path_myerr'},"CURRENT_TEST: $tname\n");
  if ( $master->[1]->{'pid'} )
  {
    mtr_tofile($master->[1]->{'path_myerr'},"CURRENT_TEST: $tname\n");
  }
}

sub do_after_run_mysqltest($)
{
  my $tinfo= shift;
  my $tname= $tinfo->{'name'};

  #MASV cleanup
    # Save info from this testcase run to mysqltest.log
    my $testcase_log= mtr_fromfile($path_timefile) if -f $path_timefile;
    mtr_tofile($path_mysqltest_log,"CURRENT TEST $tname\n");
    mtr_tofile($path_mysqltest_log, $testcase_log);
  }


##############################################################################
#
#  Run a single test case
#
##############################################################################

# When we get here, we have already filtered out test cases that doesn't
# apply to the current setup, for example if we use a running server, test
# cases that restart the server are dropped. So this function should mostly
# be about doing things, not a lot of logic.

# We don't start and kill the servers for each testcase. But some
# testcases needs a restart, because they specify options to start
# mysqld with. After that testcase, we need to restart again, to set
# back the normal options.

sub run_testcase ($) {
  my $tinfo=  shift;

  my $master_restart= run_testcase_need_master_restart($tinfo);
  my $slave_restart= run_testcase_need_slave_restart($tinfo);

  if ($master_restart or $slave_restart)
  {
    run_testcase_stop_servers($tinfo, $master_restart, $slave_restart);
  }

  my $died= mtr_record_dead_children();
  if ($died or $master_restart or $slave_restart)
  {
    run_testcase_start_servers($tinfo);
  }

  # ----------------------------------------------------------------------
  # If --start-and-exit or --start-dirty given, stop here to let user manually
  # run tests
  # ----------------------------------------------------------------------
  if ( $opt_start_and_exit or $opt_start_dirty )
  {
    mtr_report("\nServers started, exiting");
    exit(0);
  }

  {
    do_before_run_mysqltest($tinfo);

    my $res= run_mysqltest($tinfo);
    mtr_report_test_name($tinfo);
    if ( $res == 0 )
    {
      mtr_report_test_passed($tinfo);
    }
    elsif ( $res == 62 )
    {
      # Testcase itself tell us to skip this one

      # Try to get reason from mysqltest.log
      my $last_line= mtr_lastlinefromfile($path_timefile) if -f $path_timefile;
      my $reason= mtr_match_prefix($last_line, "reason: ");
      $tinfo->{'comment'}=
	defined $reason ? $reason : "Detected by testcase(reason unknown) ";
      mtr_report_test_skipped($tinfo);
    }
    elsif ( $res == 63 )
    {
      $tinfo->{'timeout'}= 1;           # Mark as timeout
      report_failure_and_restart($tinfo);
    }
    else
    {
      # Test case failed, if in control mysqltest returns 1
      if ( $res != 1 )
      {
        mtr_tofile($path_timefile,
                   "mysqltest returned unexpected code $res, " .
                   "it has probably crashed");
      }

      report_failure_and_restart($tinfo);
    }

    do_after_run_mysqltest($tinfo);
  }

  # ----------------------------------------------------------------------
  # Stop Instance Manager if we are processing an IM-test case.
  # ----------------------------------------------------------------------

  if ( ! $glob_use_running_server and $tinfo->{'component_id'} eq 'im' )
  {
    mtr_im_stop($instance_manager, $tinfo->{'name'});
  }
}


#
# Save a snapshot of the installed test db(s)
# I.e take a snapshot of the var/ dir
#
sub save_installed_db () {

  mtr_report("Saving snapshot of installed databases");
  rmtree($path_snapshot);

  foreach my $data_dir (@data_dir_lst)
  {
    my $name= basename($data_dir);
    mtr_copy_dir("$data_dir", "$path_snapshot/$name");
  }
}


#
# Save any interesting files in the data_dir
# before the data dir is removed.
#
sub save_files_before_restore($$) {
  my $test_name= shift;
  my $data_dir= shift;
  my $save_name= "$opt_vardir/log/$test_name";

  # Look for core files
  foreach my $core_file ( glob("$data_dir/core*") )
  {
    my $core_name= basename($core_file);
    mtr_report("Saving $core_name");
    mkdir($save_name) if ! -d $save_name;
    rename("$core_file", "$save_name/$core_name");
  }
}


#
# Restore snapshot of the installed test db(s)
# if the snapshot exists
#
sub restore_installed_db ($) {
  my $test_name= shift;

  if ( -d $path_snapshot)
  {
    mtr_report("Restoring snapshot of databases");

    foreach my $data_dir (@data_dir_lst)
    {
      my $name= basename($data_dir);
      save_files_before_restore($test_name, $data_dir);
      rmtree("$data_dir");
      mtr_copy_dir("$path_snapshot/$name", "$data_dir");
    }

    # Remove the ndb_*_fs dirs for all ndbd nodes
    # forcing a clean start of ndb
    foreach my $cluster (@{$clusters})
    {
      foreach my $ndbd (@{$cluster->{'ndbds'}})
      {
	rmtree("$ndbd->{'path_fs'}" );
      }
    }
  }
  else
  {
    # No snapshot existed
    mtr_error("No snapshot existed");
  }
}

sub report_failure_and_restart ($) {
  my $tinfo= shift;

  mtr_report_test_failed($tinfo);
  mtr_show_failed_diff($tinfo->{'name'});
  print "\n";
  if ( $opt_force )
  {
    # Stop all servers that are known to be running
    stop_all_servers();

    # Restore the snapshot of the installed test db
    restore_installed_db($tinfo->{'name'});
    print "Resuming Tests\n\n";
    return;
  }

  my $test_mode= join(" ", @::glob_test_mode) || "default";
  print "Aborting: $tinfo->{'name'} failed in $test_mode mode. ";
  print "To continue, re-run with '--force'.\n";
  if ( ! $glob_debugger and
       ! $glob_use_running_server and
       ! $glob_use_embedded_server )
  {
    stop_all_servers();
  }
  mtr_exit(1);

}


##############################################################################
#
#  Start and stop servers
#
##############################################################################


# The embedded server needs the cleanup so we do some of the start work
# but stop before actually running mysqld or anything.
sub do_before_start_master ($$) {
  my $tname=       shift;
  my $init_script= shift;

  # FIXME what about second master.....

  # Remove stale binary logs except for 2 tests which need them FIXME here????
  if ( $tname ne "rpl_crash_binlog_ib_1b" and
       $tname ne "rpl_crash_binlog_ib_2b" and
       $tname ne "rpl_crash_binlog_ib_3b")
  {
    # FIXME we really want separate dir for binlogs
    foreach my $bin ( glob("$opt_vardir/log/master*-bin*") )
    {
      unlink($bin);
    }
  }

  # FIXME only remove the ones that are tied to this master
  # Remove old master.info and relay-log.info files
  unlink("$master->[0]->{'path_myddir'}/master.info");
  unlink("$master->[0]->{'path_myddir'}/relay-log.info");
  unlink("$master->[1]->{'path_myddir'}/master.info");
  unlink("$master->[1]->{'path_myddir'}/relay-log.info");

  # Run master initialization shell script if one exists
  if ( $init_script )
  {
    my $ret= mtr_run("/bin/sh", [$init_script], "", "", "", "");
    if ( $ret != 0 )
    {
      # FIXME rewrite those scripts to return 0 if successful
      # mtr_warning("$init_script exited with code $ret");
    }
  }
  # for gcov  FIXME needed? If so we need more absolute paths
  # chdir($glob_basedir);
}


sub do_before_start_slave ($$) {
  my $tname=       shift;
  my $init_script= shift;

  # Remove stale binary logs and old master.info files
  # except for too tests which need them
  if ( $tname ne "rpl_crash_binlog_ib_1b" and
       $tname ne "rpl_crash_binlog_ib_2b" and
       $tname ne "rpl_crash_binlog_ib_3b" )
  {
    # FIXME we really want separate dir for binlogs
    foreach my $bin ( glob("$opt_vardir/log/slave*-bin*") )
    {
      unlink($bin);
    }
    # FIXME really master?!
    unlink("$slave->[0]->{'path_myddir'}/master.info");
    unlink("$slave->[0]->{'path_myddir'}/relay-log.info");
  }

  # Run slave initialization shell script if one exists
  if ( $init_script )
  {
    my $ret= mtr_run("/bin/sh", [$init_script], "", "", "", "");
    if ( $ret != 0 )
    {
      # FIXME rewrite those scripts to return 0 if successful
      # mtr_warning("$init_script exited with code $ret");
    }
  }

  foreach my $bin ( glob("$slave->[0]->{'path_myddir'}/log.*") )
  {
    unlink($bin);
  }
}


sub mysqld_arguments ($$$$$) {
  my $args=              shift;
  my $type=              shift;
  my $idx=               shift;
  my $extra_opt=         shift;
  my $slave_master_info= shift;

  my $sidx= "";                 # Index as string, 0 is empty string
  if ( $idx > 0 )
  {
    $sidx= "$idx";
  }

  my $prefix= "";               # If mysqltest server arg

  if ( $glob_use_embedded_server )
  {
    $prefix= "--server-arg=";
  } else {
    # We can't pass embedded server --no-defaults
    mtr_add_arg($args, "%s--no-defaults", $prefix);
  }

  mtr_add_arg($args, "%s--console", $prefix);
  mtr_add_arg($args, "%s--basedir=%s", $prefix, $path_my_basedir);
  mtr_add_arg($args, "%s--character-sets-dir=%s", $prefix, $path_charsetsdir);
  mtr_add_arg($args, "%s--core", $prefix);
  mtr_add_arg($args, "%s--log-bin-trust-function-creators", $prefix);
  mtr_add_arg($args, "%s--default-character-set=latin1", $prefix);
  mtr_add_arg($args, "%s--language=%s", $prefix, $path_language);
  mtr_add_arg($args, "%s--tmpdir=$opt_tmpdir", $prefix);

  if ( $opt_valgrind_mysqld )
  {
    mtr_add_arg($args, "%s--skip-safemalloc", $prefix);
  }

  my $pidfile;

  if ( $type eq 'master' )
  {
    my $id= $idx > 0 ? $idx + 101 : 1;

    if (! $opt_skip_master_binlog)
    {
      mtr_add_arg($args, "%s--log-bin=%s/log/master-bin%s", $prefix,
                  $opt_vardir, $sidx);
    }
    mtr_add_arg($args, "%s--pid-file=%s", $prefix,
                $master->[$idx]->{'path_pid'});
    mtr_add_arg($args, "%s--port=%d", $prefix,
                $master->[$idx]->{'port'});
    mtr_add_arg($args, "%s--server-id=%d", $prefix, $id);
    mtr_add_arg($args, "%s--socket=%s", $prefix,
                $master->[$idx]->{'path_sock'});
    mtr_add_arg($args, "%s--innodb_data_file_path=ibdata1:10M:autoextend", $prefix);
    mtr_add_arg($args, "%s--local-infile", $prefix);
    mtr_add_arg($args, "%s--datadir=%s", $prefix,
                $master->[$idx]->{'path_myddir'});

    if ( $idx > 0 or !$use_innodb)
    {
      mtr_add_arg($args, "%s--skip-innodb", $prefix);
    }

    my $cluster= $clusters->[$master->[$idx]->{'cluster'}];
    if ( $opt_skip_ndbcluster ||
	 !$cluster->{'pid'})
    {
      mtr_add_arg($args, "%s--skip-ndbcluster", $prefix);
    }
    else
    {
      mtr_add_arg($args, "%s--ndbcluster", $prefix);
      mtr_add_arg($args, "%s--ndb-connectstring=%s", $prefix,
		  $cluster->{'connect_string'});
      mtr_add_arg($args, "%s--ndb-extra-logging", $prefix);
    }
  }

  if ( $type eq 'slave' )
  {
    my $slave_server_id=  2 + $idx;
    my $slave_rpl_rank= $slave_server_id;

    mtr_add_arg($args, "%s--datadir=%s", $prefix,
                $slave->[$idx]->{'path_myddir'});
    # FIXME slave get this option twice?!
    mtr_add_arg($args, "%s--exit-info=256", $prefix);
    mtr_add_arg($args, "%s--init-rpl-role=slave", $prefix);
    if (! $opt_skip_slave_binlog)
    {
      mtr_add_arg($args, "%s--log-bin=%s/log/slave%s-bin", $prefix,
                  $opt_vardir, $sidx); # FIXME use own dir for binlogs
      mtr_add_arg($args, "%s--log-slave-updates", $prefix);
    }
    # FIXME option duplicated for slave
    mtr_add_arg($args, "%s--log=%s", $prefix,
                $slave->[$idx]->{'path_mylog'});
    mtr_add_arg($args, "%s--master-retry-count=10", $prefix);
    mtr_add_arg($args, "%s--pid-file=%s", $prefix,
                $slave->[$idx]->{'path_pid'});
    mtr_add_arg($args, "%s--port=%d", $prefix,
                $slave->[$idx]->{'port'});
    mtr_add_arg($args, "%s--relay-log=%s/log/slave%s-relay-bin", $prefix,
                $opt_vardir, $sidx);
    mtr_add_arg($args, "%s--report-host=127.0.0.1", $prefix);
    mtr_add_arg($args, "%s--report-port=%d", $prefix,
                $slave->[$idx]->{'port'});
    mtr_add_arg($args, "%s--report-user=root", $prefix);
    mtr_add_arg($args, "%s--skip-innodb", $prefix);
    mtr_add_arg($args, "%s--skip-ndbcluster", $prefix);
    mtr_add_arg($args, "%s--skip-slave-start", $prefix);

    # Directory where slaves find the dumps generated by "load data"
    # on the server. The path need to have constant length otherwise
    # test results will vary, thus a relative path is used.
    mtr_add_arg($args, "%s--slave-load-tmpdir=%s", $prefix,
                "../tmp");
    mtr_add_arg($args, "%s--socket=%s", $prefix,
                $slave->[$idx]->{'path_sock'});
    mtr_add_arg($args, "%s--set-variable=slave_net_timeout=10", $prefix);

    if ( @$slave_master_info )
    {
      foreach my $arg ( @$slave_master_info )
      {
        mtr_add_arg($args, "%s%s", $prefix, $arg);
      }
    }
    else
    {
      mtr_add_arg($args, "%s--master-user=root", $prefix);
      mtr_add_arg($args, "%s--master-connect-retry=1", $prefix);
      mtr_add_arg($args, "%s--master-host=127.0.0.1", $prefix);
      mtr_add_arg($args, "%s--master-password=", $prefix);
      mtr_add_arg($args, "%s--master-port=%d", $prefix,
                  $master->[0]->{'port'}); # First master
      mtr_add_arg($args, "%s--server-id=%d", $prefix, $slave_server_id);
      mtr_add_arg($args, "%s--rpl-recovery-rank=%d", $prefix, $slave_rpl_rank);
    }

    if ( $opt_skip_ndbcluster_slave ||
         $slave->[$idx]->{'cluster'} == -1 ||
	 !$clusters->[$slave->[$idx]->{'cluster'}]->{'pid'} )
    {
      mtr_add_arg($args, "%s--skip-ndbcluster", $prefix);
    }
    else
    {
      mtr_add_arg($args, "%s--ndbcluster", $prefix);
      mtr_add_arg($args, "%s--ndb-connectstring=%s", $prefix,
		  $clusters->[$slave->[$idx]->{'cluster'}]->{'connect_string'});
      mtr_add_arg($args, "%s--ndb-extra-logging", $prefix);
    }
  } # end slave

  if ( $opt_debug )
  {
    if ( $type eq 'master' )
    {
      mtr_add_arg($args, "%s--debug=d:t:i:A,%s/log/master%s.trace",
                  $prefix, $opt_vardir_trace, $sidx);
    }
    if ( $type eq 'slave' )
    {
      mtr_add_arg($args, "%s--debug=d:t:i:A,%s/log/slave%s.trace",
                  $prefix, $opt_vardir_trace, $sidx);
    }
  }

  # FIXME always set nowdays??? SMALL_SERVER
  mtr_add_arg($args, "%s--key_buffer_size=1M", $prefix);
  mtr_add_arg($args, "%s--sort_buffer=256K", $prefix);
  mtr_add_arg($args, "%s--max_heap_table_size=1M", $prefix);
  mtr_add_arg($args, "%s--log-bin-trust-function-creators", $prefix);

  if ( $opt_ssl_supported )
  {
    mtr_add_arg($args, "%s--ssl-ca=%s/std_data/cacert.pem", $prefix,
                $glob_mysql_test_dir);
    mtr_add_arg($args, "%s--ssl-cert=%s/std_data/server-cert.pem", $prefix,
                $glob_mysql_test_dir);
    mtr_add_arg($args, "%s--ssl-key=%s/std_data/server-key.pem", $prefix,
                $glob_mysql_test_dir);
  }

  if ( $opt_warnings )
  {
    mtr_add_arg($args, "%s--log-warnings", $prefix);
  }

  # Indicate to "mysqld" it will be debugged in debugger
  if ( $glob_debugger )
  {
    mtr_add_arg($args, "%s--gdb", $prefix);
  }

  # If we should run all tests cases, we will use a local server for that

  if ( -w "/" )
  {
    # We are running as root;  We need to add the --root argument
    mtr_add_arg($args, "%s--user=root", $prefix);
  }

  foreach my $arg ( @opt_extra_mysqld_opt, @$extra_opt )
  {
    mtr_add_arg($args, "%s%s", $prefix, $arg);
  }

  if ( $opt_bench )
  {
    mtr_add_arg($args, "%s--rpl-recovery-rank=1", $prefix);
    mtr_add_arg($args, "%s--init-rpl-role=master", $prefix);
  }
  elsif ( $type eq 'master' )
  {
    mtr_add_arg($args, "%s--exit-info=256", $prefix);
    mtr_add_arg($args, "%s--open-files-limit=1024", $prefix);
    mtr_add_arg($args, "%s--log=%s", $prefix, $master->[0]->{'path_mylog'});
  }

  return $args;
}


##############################################################################
#
#  Start mysqld and return the PID
#
##############################################################################

sub mysqld_start ($$$) {
  my $mysqld=            shift;
  my $extra_opt=         shift;
  my $slave_master_info= shift;

  my $args;                             # Arg vector
  my $exe;
  my $pid= -1;
  my $wait_for_pid_file= 1;

  my $type= $mysqld->{'type'};
  my $idx= $mysqld->{'idx'};

  if ( $type eq 'master' )
  {
    $exe= $exe_master_mysqld;
  }
  if ( $type eq 'slave' )
  {
    $exe= $exe_slave_mysqld;
  }
  else
  {
    $exe= $exe_mysqld;
  }

  mtr_init_args(\$args);

  if ( $opt_valgrind_mysqld )
  {
    valgrind_arguments($args, \$exe);
  }

  mysqld_arguments($args,$type,$idx,$extra_opt,$slave_master_info);

  if ( $opt_gdb || $opt_manual_gdb)
  {
    gdb_arguments(\$args, \$exe, "$type"."_$idx");
  }
  elsif ( $opt_ddd || $opt_manual_ddd )
  {
    ddd_arguments(\$args, \$exe, "$type"."_$idx");
  }
  elsif ( $opt_debugger )
  {
    debugger_arguments(\$args, \$exe, "$type"."_$idx");
  }
  elsif ( $opt_manual_debug )
  {
     print "\nStart $type in your debugger\n" .
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

  if ($exe_libtool and $opt_valgrind)
  {
    # Add "libtool --mode-execute"
    # if running in valgrind(to avoid valgrinding bash)
    unshift(@$args, "--mode=execute", $exe);
    $exe= $exe_libtool;
  }


  if ( defined $exe )
  {
    $pid= mtr_spawn($exe, $args, "",
		    $mysqld->{'path_myerr'},
		    $mysqld->{'path_myerr'},
		    "",
		    { append_log_file => 1 });
  }


  if ( $wait_for_pid_file && !sleep_until_file_created($mysqld->{'path_pid'},
						       $mysqld->{'start_timeout'},
						       $pid))
  {

    mtr_error("Failed to start mysqld $mysqld->{'type'}");
  }


  # Remember pid of the started process
  $mysqld->{'pid'}= $pid;

  # Remember options used when starting
  $mysqld->{'start_opts'}= $extra_opt;
  $mysqld->{'start_slave_master_info'}= $slave_master_info;

  mtr_verbose("mysqld pid: $pid");
  return $pid;
}


sub stop_all_servers () {

  print  "Stopping All Servers\n";

  print  "Shutting-down Instance Manager\n";
  mtr_im_stop($instance_manager, "stop_all_servers");

  my %admin_pids; # hash of admin processes that requests shutdown
  my @kill_pids;  # list of processes to shutdown/kill
  my $pid;

  # Start shutdown of all started masters
  foreach my $mysqld (@{$master}, @{$slave})
  {
    if ( $mysqld->{'pid'} )
    {
      $pid= mtr_mysqladmin_start($mysqld, "shutdown", 70);
      $admin_pids{$pid}= 1;

      push(@kill_pids,{
		       pid      => $mysqld->{'pid'},
		       pidfile  => $mysqld->{'path_pid'},
		       sockfile => $mysqld->{'path_sock'},
		       port     => $mysqld->{'port'},
		      });

      $mysqld->{'pid'}= 0; # Assume we are done with it
    }
  }

  # Start shutdown of clusters
  foreach my $cluster (@{$clusters})
  {
    if ( $cluster->{'pid'} )
    {
      $pid= mtr_ndbmgm_start($cluster, "shutdown");
      $admin_pids{$pid}= 1;

      push(@kill_pids,{
		       pid      => $cluster->{'pid'},
		       pidfile  => $cluster->{'path_pid'}
		      });

      $cluster->{'pid'}= 0; # Assume we are done with it

      foreach my $ndbd (@{$cluster->{'ndbds'}})
      {
        if ( $ndbd->{'pid'} )
	{
	  push(@kill_pids,{
			   pid      => $ndbd->{'pid'},
			   pidfile  => $ndbd->{'path_pid'},
			  });
	  $ndbd->{'pid'}= 0;
	}
      }
    }
  }

  # Wait blocking until all shutdown processes has completed
  mtr_wait_blocking(\%admin_pids);

  # Make sure that process has shutdown else try to kill them
  mtr_check_stop_servers(\@kill_pids);

  foreach my $mysqld (@{$master}, @{$slave})
  {
    rm_ndbcluster_tables($mysqld->{'path_myddir'});
  }
}


sub run_testcase_need_master_restart($)
{
  my ($tinfo)= @_;

  # We try to find out if we are to restart the master(s)
  my $do_restart= 0;          # Assumes we don't have to

  if ( $tinfo->{'master_sh'} )
  {
    $do_restart= 1;           # Always restart if script to run
    mtr_verbose("Restart because: Always restart if script to run");
  }
  elsif ( ! $opt_skip_ndbcluster and
	  $tinfo->{'ndb_test'} == 0 and
	  $clusters->[0]->{'pid'} != 0 )
  {
    $do_restart= 1;           # Restart without cluster
    mtr_verbose("Restart because: Test does not need cluster");
  }
  elsif ( ! $opt_skip_ndbcluster and
	  $tinfo->{'ndb_test'} == 1 and
	  $clusters->[0]->{'pid'} == 0 )
  {
    $do_restart= 1;           # Restart with cluster
    mtr_verbose("Restart because: Test need cluster");
  }
  elsif ( $master->[0]->{'running_master_is_special'} and
	  $master->[0]->{'running_master_is_special'}->{'timezone'} eq
	  $tinfo->{'timezone'} and
	  mtr_same_opts($master->[0]->{'running_master_is_special'}->{'master_opt'},
			$tinfo->{'master_opt'}) )
  {
    # If running master was started with special settings, but
    # the current test requires the same ones, we *don't* restart.
    $do_restart= 0;
    mtr_verbose("Skip restart: options are equal " .
	       join(" ", @{$tinfo->{'master_opt'}}));
  }
  elsif ( $tinfo->{'master_restart'} )
  {
    $do_restart= 1;
    mtr_verbose("Restart because: master_restart");
  }
  elsif ( $master->[0]->{'running_master_is_special'} )
  {
    $do_restart= 1;
    mtr_verbose("Restart because: running_master_is_special");
  }
  # Check that running master was started with same options
  # as the current test requires
  elsif (! mtr_same_opts($master->[0]->{'start_opts'},
                         $tinfo->{'master_opt'}) )
  {
    $do_restart= 1;
    mtr_verbose("Restart because: running with different options '" .
	       join(" ", @{$tinfo->{'master_opt'}}) . "' != '" .
		join(" ", @{$master->[0]->{'start_opts'}}) . "'" );
  }

  return $do_restart;
}

sub run_testcase_need_slave_restart($)
{
  my ($tinfo)= @_;

  # We try to find out if we are to restart the slaves
  my $do_slave_restart= 0;     # Assumes we don't have to

  # FIXME only restart slave when necessary
  $do_slave_restart= 1;

#   if ( ! $slave->[0]->{'pid'} )
#   {
#     # mtr_verbose("Slave not started, no need to check slave restart");
#   }
#   elsif ( $do_restart )
#   {
#     $do_slave_restart= 1;      # Always restart if master restart
#     mtr_verbose("Restart slave because: Master restart");
#   }
#   elsif ( $tinfo->{'slave_sh'} )
#   {
#     $do_slave_restart= 1;      # Always restart if script to run
#     mtr_verbose("Restart slave because: Always restart if script to run");
#   }
#   elsif ( ! $opt_skip_ndbcluster_slave and
# 	  $tinfo->{'ndb_test'} == 0 and
# 	  $clusters->[1]->{'pid'} != 0 )
#   {
#     $do_slave_restart= 1;       # Restart without slave cluster
#     mtr_verbose("Restart slave because: Test does not need slave cluster");
#   }
#   elsif ( ! $opt_with_ndbcluster_slave and
# 	  $tinfo->{'ndb_test'} == 1 and
# 	  $clusters->[1]->{'pid'} == 0 )
#   {
#     $do_slave_restart= 1;       # Restart with slave cluster
#     mtr_verbose("Restart slave because: Test need slave cluster");
#   }
#   elsif ( $tinfo->{'slave_restart'} )
#   {
#     $do_slave_restart= 1;
#     mtr_verbose("Restart slave because: slave_restart");
#   }
#   elsif ( $slave->[0]->{'running_slave_is_special'} )
#   {
#     $do_slave_restart= 1;
#     mtr_verbose("Restart slave because: running_slave_is_special");
#   }
#   # Check that running slave was started with same options
#   # as the current test requires
#   elsif (! mtr_same_opts($slave->[0]->{'start_opts'},
#                          $tinfo->{'slave_opt'}) )
#   {
#     $do_slave_restart= 1;
#     mtr_verbose("Restart slave because: running with different options '" .
# 	       join(" ", @{$tinfo->{'slave_opt'}}) . "' != '" .
# 		join(" ", @{$slave->[0]->{'start_opts'}}) . "'" );
#   }

  return $do_slave_restart;

}

# ----------------------------------------------------------------------
# If not using a running servers we may need to stop and restart.
# We restart in the case we have initiation scripts, server options
# etc to run. But we also restart again after the test first restart
# and test is run, to get back to normal server settings.
#
# To make the code a bit more clean, we actually only stop servers
# here, and mark this to be done. Then a generic "start" part will
# start up the needed servers again.
# ----------------------------------------------------------------------

sub run_testcase_stop_servers($$$) {
  my ($tinfo, $do_restart, $do_slave_restart)= @_;

  if ( $glob_use_running_server || $glob_use_embedded_server )
  {
      return;
  }

  my $pid;
  my %admin_pids; # hash of admin processes that requests shutdown
  my @kill_pids;  # list of processes to shutdown/kill


  # Remember if we restarted for this test case
  $tinfo->{'restarted'}= $do_restart;

  if ( $do_restart )
  {
    delete $master->[0]->{'running_master_is_special'}; # Forget history

    # Start shutdown of all started masters
    foreach my $mysqld (@{$master})
    {
      if ( $mysqld->{'pid'} )
      {
	$pid= mtr_mysqladmin_start($mysqld, "shutdown", 70);

	$admin_pids{$pid}= 1;

	push(@kill_pids,{
			 pid      => $mysqld->{'pid'},
			 pidfile  => $mysqld->{'path_pid'},
			 sockfile => $mysqld->{'path_sock'},
			 port     => $mysqld->{'port'},
			});

	$mysqld->{'pid'}= 0; # Assume we are done with it
      }
    }

    # Start shutdown of master cluster
    my $cluster= $clusters->[0];
    if ( $cluster->{'pid'} )
    {
      $pid= mtr_ndbmgm_start($cluster, "shutdown");
      $admin_pids{$pid}= 1;

      push(@kill_pids,{
		       pid      => $cluster->{'pid'},
		       pidfile  => $cluster->{'path_pid'}
		      });

      $cluster->{'pid'}= 0; # Assume we are done with it

      foreach my $ndbd (@{$cluster->{'ndbds'}})
      {
	push(@kill_pids,{
			 pid      => $ndbd->{'pid'},
			 pidfile  => $ndbd->{'path_pid'},
			});
	$ndbd->{'pid'}= 0; # Assume we are done with it
      }
    }
  }

  if ( $do_restart || $do_slave_restart )
  {

    delete $slave->[0]->{'running_slave_is_special'}; # Forget history

    # Start shutdown of all started slaves
    foreach my $mysqld (@{$slave})
    {
      if ( $mysqld->{'pid'} )
      {
	$pid= mtr_mysqladmin_start($mysqld, "shutdown", 70);

	$admin_pids{$pid}= 1;

	push(@kill_pids,{
			 pid      => $mysqld->{'pid'},
			 pidfile  => $mysqld->{'path_pid'},
			 sockfile => $mysqld->{'path_sock'},
			 port     => $mysqld->{'port'},
			});


	$mysqld->{'pid'}= 0; # Assume we are done with it
      }
    }

    # Start shutdown of slave cluster
    my $cluster= $clusters->[1];
    if ( $cluster->{'pid'} )
    {
      $pid= mtr_ndbmgm_start($cluster, "shutdown");

      $admin_pids{$pid}= 1;

      push(@kill_pids,{
		       pid      => $cluster->{'pid'},
		       pidfile  => $cluster->{'path_pid'}
		      });

      $cluster->{'pid'}= 0; # Assume we are done with it

      foreach my $ndbd (@{$cluster->{'ndbds'}} )
      {
	push(@kill_pids,{
			 pid      => $ndbd->{'pid'},
			 pidfile  => $ndbd->{'path_pid'},
			});
	$ndbd->{'pid'}= 0; # Assume we are done with it
      }
    }
  }

  # ----------------------------------------------------------------------
  # Shutdown has now been started and lists for the shutdown processes
  # and the processes to be killed has been created
  # ----------------------------------------------------------------------

  # Wait blocking until all shutdown processes has completed
  mtr_wait_blocking(\%admin_pids);


  # Make sure that process has shutdown else try to kill them
  mtr_check_stop_servers(\@kill_pids);

  foreach my $mysqld (@{$master}, @{$slave})
  {
    if ( ! $mysqld->{'pid'} )
    {
      # Remove ndbcluster tables if server is stopped
      rm_ndbcluster_tables($mysqld->{'path_myddir'});
    }
  }
}

sub run_testcase_start_servers($) {
  my $tinfo= shift;

  my $tname= $tinfo->{'name'};

  if ( $glob_use_running_server or $glob_use_embedded_server )
  {
    return;
  }

  if ( $tinfo->{'component_id'} eq 'mysqld' )
  {
    if ( ! $opt_skip_ndbcluster and
	 !$clusters->[0]->{'pid'} and
	 $tinfo->{'ndb_test'} )
    {
      # Test need cluster, cluster is not started, start it
      ndbcluster_start($clusters->[0], "");
    }

    if ( !$master->[0]->{'pid'} )
    {
      # Master mysqld is not started
      do_before_start_master($tname,$tinfo->{'master_sh'});

      mysqld_start($master->[0],$tinfo->{'master_opt'},[]);

    }

    if ( $clusters->[0]->{'pid'} and ! $master->[1]->{'pid'} )
    {
      # Test needs cluster, start an extra mysqld connected to cluster

      # First wait for first mysql server to have created ndb system tables ok
      # FIXME This is a workaround so that only one mysqld creates the tables
      if ( ! sleep_until_file_created(
	     "$master->[0]->{'path_myddir'}/cluster/apply_status.ndb",
				      $master->[0]->{'start_timeout'},
				      $master->[0]->{'pid'}))
      {
	mtr_report("Failed to create 'cluster/apply_status' table");
	report_failure_and_restart($tinfo);
	return;
      }
      mtr_tofile($master->[1]->{'path_myerr'},"CURRENT_TEST: $tname\n");

      mysqld_start($master->[1],$tinfo->{'master_opt'},[]);
    }

    if ( $tinfo->{'master_restart'} )
    {
      # Save this test case information, so next can examine it
      $master->[0]->{'running_master_is_special'}= $tinfo;
    }
  }
  elsif ( ! $opt_skip_im and $tinfo->{'component_id'} eq 'im' )
  {
    # We have to create defaults file every time, in order to ensure that it
    # will be the same for each test. The problem is that test can change the
    # file (by SET/UNSET commands), so w/o recreating the file, execution of
    # one test can affect the other.

    im_create_defaults_file($instance_manager);

    mtr_im_start($instance_manager, $tinfo->{im_opts});
  }

  # ----------------------------------------------------------------------
  # Start slaves - if needed
  # ----------------------------------------------------------------------
  if ( $tinfo->{'slave_num'} )
  {
    mtr_tofile($slave->[0]->{'path_myerr'},"CURRENT_TEST: $tname\n");

    do_before_start_slave($tname,$tinfo->{'slave_sh'});

    if ( ! $opt_skip_ndbcluster_slave and
	 !$clusters->[1]->{'pid'} and
	 $tinfo->{'ndb_test'} )
    {
      # Test need slave cluster, cluster is not started, start it
      ndbcluster_start($clusters->[1], "");
    }

    for ( my $idx= 0; $idx <  $tinfo->{'slave_num'}; $idx++ )
    {
      if ( ! $slave->[$idx]->{'pid'} )
      {
	mysqld_start($slave->[$idx],$tinfo->{'slave_opt'},
		     $tinfo->{'slave_mi'});

      }
    }

    if ( $tinfo->{'slave_restart'} )
    {
      # Save this test case information, so next can examine it
      $slave->[0]->{'running_slave_is_special'}= $tinfo;
    }

  }

  # Wait for clusters to start
  foreach my $cluster (@{$clusters})
  {

    next if !$cluster->{'pid'};

    if (ndbcluster_wait_started($cluster, ""))
    {
      # failed to start
      mtr_report("Start of $cluster->{'name'} cluster failed, ");
    }
  }

  # Wait for mysqld's to start
  foreach my $mysqld (@{$master},@{$slave})
  {

    next if !$mysqld->{'pid'};

    if (mysqld_wait_started($mysqld))
    {
      mtr_warning("Failed to start $mysqld->{'type'} mysqld $mysqld->{'idx'}");
    }
  }
}

#
# Run include/check-testcase.test
# Before a testcase, run in record mode, save result file to var
# After testcase, run and compare with the recorded file, they should be equal!
#
sub run_check_testcase ($$) {

  my $mode=     shift;
  my $mysqld=   shift;

  my $name= "check-" . $mysqld->{'type'} . $mysqld->{'idx'};

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--silent");
  mtr_add_arg($args, "-v");
  mtr_add_arg($args, "--skip-safemalloc");
  mtr_add_arg($args, "--tmpdir=%s", $opt_tmpdir);

  mtr_add_arg($args, "--socket=%s", $mysqld->{'path_sock'});
  mtr_add_arg($args, "--port=%d", $mysqld->{'port'});
  mtr_add_arg($args, "--database=test");
  mtr_add_arg($args, "--user=%s", $opt_user);
  mtr_add_arg($args, "--password=");

  mtr_add_arg($args, "-R");
  mtr_add_arg($args, "$opt_vardir/tmp/$name.result");

  if ( $mode eq "before" )
  {
    mtr_add_arg($args, "--record");
  }

  my $res = mtr_run_test($exe_mysqltest,$args,
	        "include/check-testcase.test", "", "", "");

  if ( $res == 1  and $mode = "after")
  {
    mtr_run("diff",["-u",
		    "$opt_vardir/tmp/$name.result",
		    "$opt_vardir/tmp/$name.reject"],
	    "", "", "", "");
  }
  elsif ( $res )
  {
    mtr_error("Could not execute 'check-testcase' $mode testcase");
  }
}



sub run_mysqltest ($) {
  my ($tinfo)= @_;

  my $exe= $exe_mysqltest;
  my $args;

  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--silent");
  mtr_add_arg($args, "-v");
  mtr_add_arg($args, "--skip-safemalloc");
  mtr_add_arg($args, "--tmpdir=%s", $opt_tmpdir);

  if ($tinfo->{'component_id'} eq 'im')
  {
    mtr_add_arg($args, "--socket=%s", $instance_manager->{'path_sock'});
    mtr_add_arg($args, "--port=%d", $instance_manager->{'port'});
    mtr_add_arg($args, "--user=%s", $instance_manager->{'admin_login'});
    mtr_add_arg($args, "--password=%s", $instance_manager->{'admin_password'});
  }
  else # component_id == mysqld
  {
    mtr_add_arg($args, "--socket=%s", $master->[0]->{'path_sock'});
    mtr_add_arg($args, "--port=%d", $master->[0]->{'port'});
    mtr_add_arg($args, "--database=test");
    mtr_add_arg($args, "--user=%s", $opt_user);
    mtr_add_arg($args, "--password=");
  }

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

  if ( $opt_strace_client )
  {
    $exe=  "strace";            # FIXME there are ktrace, ....
    mtr_add_arg($args, "-o");
    mtr_add_arg($args, "%s/log/mysqltest.strace", $opt_vardir);
    mtr_add_arg($args, "$exe_mysqltest");
  }

  if ( $opt_timer )
  {
    mtr_add_arg($args, "--timer-file=%s/log/timer", $opt_vardir);
  }

  if ( $opt_big_test )
  {
    mtr_add_arg($args, "--big-test");
  }

  if ( $opt_compress )
  {
    mtr_add_arg($args, "--compress");
  }

  if ( $opt_sleep )
  {
    mtr_add_arg($args, "--sleep=%d", $opt_sleep);
  }

  if ( $opt_debug )
  {
    mtr_add_arg($args, "--debug=d:t:A,%s/log/mysqltest.trace",
		$opt_vardir_trace);
  }

  if ( $opt_ssl_supported )
  {
    mtr_add_arg($args, "--ssl-ca=%s/std_data/cacert.pem",
	        $glob_mysql_test_dir);
    mtr_add_arg($args, "--ssl-cert=%s/std_data/client-cert.pem",
	        $glob_mysql_test_dir);
    mtr_add_arg($args, "--ssl-key=%s/std_data/client-key.pem",
	        $glob_mysql_test_dir);
  }

  if ( $opt_ssl )
  {
    # Turn on SSL for _all_ test cases if option --ssl was used
    mtr_add_arg($args, "--ssl",
		$glob_mysql_test_dir);
  }
  elsif ( $opt_ssl_supported )
  {
    mtr_add_arg($args, "--skip-ssl",
		$glob_mysql_test_dir);
  }

  # ----------------------------------------------------------------------
  # If embedded server, we create server args to give mysqltest to pass on
  # ----------------------------------------------------------------------

  if ( $glob_use_embedded_server )
  {
    mysqld_arguments($args,'master',0,$tinfo->{'master_opt'},[]);
  }

  # ----------------------------------------------------------------------
  # export MYSQL_TEST variable containing <path>/mysqltest <args>
  # ----------------------------------------------------------------------
  $ENV{'MYSQL_TEST'}= "$exe_mysqltest " . join(" ", @$args);

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

  mtr_add_arg($args, "--test-file");
  mtr_add_arg($args, $tinfo->{'path'});

  mtr_add_arg($args, "--result-file");
  mtr_add_arg($args, $tinfo->{'result_file'});

  if ( $opt_record )
  {
    mtr_add_arg($args, "--record");
  }

  if ( $opt_client_gdb )
  {
    gdb_arguments(\$args, \$exe, "client");
  }
  elsif ( $opt_client_ddd )
  {
    ddd_arguments(\$args, \$exe, "client");
  }
  elsif ( $opt_client_debugger )
  {
    debugger_arguments(\$args, \$exe, "client");
  }

  if ($exe_libtool and $opt_valgrind)
  {
    # Add "libtool --mode-execute" before the test to execute
    # if running in valgrind(to avoid valgrinding bash)
    unshift(@$args, "--mode=execute", $exe);
    $exe= $exe_libtool;
  }

  if ( $opt_check_testcases )
  {
    foreach my $mysqld (@{$master}, @{$slave})
    {
      if ($mysqld->{'pid'})
      {
	run_check_testcase("before", $mysqld);
      }
    }
  }

  # -------------------------------------------------------
  # Init variables that change for each testcase
  # -------------------------------------------------------
  $ENV{'TZ'}= $tinfo->{'timezone'};

  my $res = mtr_run_test($exe,$args,"","",$path_timefile,"");

  if ( $opt_check_testcases )
  {
    foreach my $mysqld (@{$master}, @{$slave})
    {
      if ($mysqld->{'pid'})
      {
	run_check_testcase("after", $mysqld);
      }
    }
  }

  return $res;

}


#
# Modify the exe and args so that program is run in gdb in xterm
#
sub gdb_arguments {
  my $args= shift;
  my $exe=  shift;
  my $type= shift;

  # Write $args to gdb init file
  my $str= join(" ", @$$args);
  my $gdb_init_file= "$opt_tmpdir/gdbinit.$type";

  # Remove the old gdbinit file
  unlink($gdb_init_file);

  if ( $type eq "client" )
  {
    # write init file for client
    mtr_tofile($gdb_init_file,
	       "set args $str\n" .
	       "break main\n");
  }
  else
  {
    # write init file for mysqld
    mtr_tofile($gdb_init_file,
	       "set args $str\n" .
	       "break mysql_parse\n" .
	       "commands 1\n" .
	       "disable 1\n" .
	       "end\n" .
	       "run");
  }

  if ( $opt_manual_gdb )
  {
     print "\nTo start gdb for $type, type in another window:\n";
     print "cd $glob_mysql_test_dir;\n";
     print "gdb -x $gdb_init_file $$exe\n";

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
# Modify the exe and args so that program is run in ddd
#
sub ddd_arguments {
  my $args= shift;
  my $exe=  shift;
  my $type= shift;

  # Write $args to ddd init file
  my $str= join(" ", @$$args);
  my $gdb_init_file= "$opt_tmpdir/gdbinit.$type";

  # Remove the old gdbinit file
  unlink($gdb_init_file);

  if ( $type eq "client" )
  {
    # write init file for client
    mtr_tofile($gdb_init_file,
	       "set args $str\n" .
	       "break main\n");
  }
  else
  {
    # write init file for mysqld
    mtr_tofile($gdb_init_file,
	       "file $$exe\n" .
	       "set args $str\n" .
	       "break mysql_parse\n" .
	       "commands 1\n" .
	       "disable 1\n" .
	       "end");
  }

  if ( $opt_manual_ddd )
  {
     print "\nTo start ddd for $type, type in another window:\n";
     print "cd $glob_mysql_test_dir;\n";
     print "ddd -x $gdb_init_file $$exe\n";

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
# Modify the exe and args so that program is run in the selected debugger
#
sub debugger_arguments {
  my $args= shift;
  my $exe=  shift;
  my $debugger= $opt_debugger || $opt_client_debugger;

  # FIXME Need to change the below "eq"'s to
  # "case unsensitive string contains"
  if ( $debugger eq "vcexpress" or $debugger eq "vc")
  {
    # vc[express] /debugexe exe arg1 .. argn

    # Add /debugexe and name of the exe before args
    unshift(@$$args, "/debugexe");
    unshift(@$$args, "$$exe");

  }
  elsif ( $debugger eq "windbg" )
  {
    # windbg exe arg1 .. argn

    # Add name of the exe before args
    unshift(@$$args, "$$exe");

  }
  else
  {
    mtr_error("Unknown argument \"$debugger\" passed to --debugger");
  }

  # Set exe to debuggername
  $$exe= $debugger;
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
    mtr_add_arg($args, "--alignment=8");
    mtr_add_arg($args, "--leak-check=yes");
    mtr_add_arg($args, "--num-callers=16");
    mtr_add_arg($args, "--suppressions=%s/valgrind.supp", $glob_mysql_test_dir)
      if -f "$glob_mysql_test_dir/valgrind.supp";
  }

  # Add valgrind options, can be overriden by user
  mtr_add_arg($args, '%s', $_) for (split(' ', $opt_valgrind_options));

  mtr_add_arg($args, $$exe);

  $$exe= $opt_valgrind_path || "valgrind";
}


##############################################################################
#
#  Usage
#
##############################################################################

sub usage ($) {
  my $message= shift;

  if ( $message )
  {
    print STDERR "$message \n";
  }

  print STDERR <<HERE;

mysql-test-run [ OPTIONS ] [ TESTCASE ]

FIXME when is TESTCASE arg used or not?!

Options to control what engine/variation to run

  embedded-server       Use the embedded server, i.e. no mysqld daemons
  ps-protocol           Use the binary protocol between client and server
  cursor-protocol       Use the cursor protocol between client and server
                        (implies --ps-protocol)
  view-protocol         Create a view to execute all non updating queries
  sp-protocol           Create a stored procedure to execute all queries
  compress              Use the compressed protocol between client and server
  ssl                   Use ssl protocol between client and server
  skip-ssl              Dont start server with support for ssl connections
  bench                 Run the benchmark suite
  small-bench           Run the benchmarks with --small-tests --small-tables

Options to control directories to use
  benchdir=DIR          The directory where the benchmark suite is stored
                        (default: ../../mysql-bench)
  tmpdir=DIR            The directory where temporary files are stored
                        (default: ./var/tmp).
  vardir=DIR            The directory where files generated from the test run
                        is stored (default: ./var). Specifying a ramdisk or
                        tmpfs will speed up tests.

Options to control what test suites or cases to run

  force                 Continue to run the suite after failure
  with-ndbcluster       Use cluster in all tests
  with-ndbcluster-only  Run only tests that include "ndb" in the filename
  skip-ndb[cluster]     Skip all tests that need cluster
  skip-ndb[cluster]-slave Skip all tests that need a slave cluster
  ndb-extra             Run extra tests from ndb directory
  do-test=PREFIX        Run test cases which name are prefixed with PREFIX
  start-from=PREFIX     Run test cases starting from test prefixed with PREFIX
  suite=NAME            Run the test suite named NAME. The default is "main"
  skip-rpl              Skip the replication test cases.
  skip-im               Don't start IM, and skip the IM test cases
  skip-test=PREFIX      Skip test cases which name are prefixed with PREFIX
  big-test              Pass "--big-test" to mysqltest which will set the
                        environment variable BIG_TEST, which can be checked
                        from test cases.

Options that specify ports

  master_port=PORT      Specify the port number used by the first master
  slave_port=PORT       Specify the port number used by the first slave
  ndbcluster-port=PORT  Specify the port number used by cluster
  ndbcluster-port-slave=PORT  Specify the port number used by slave cluster

Options for test case authoring

  record TESTNAME       (Re)genereate the result file for TESTNAME
  check-testcases       Check testcases for sideeffects

Options that pass on options

  mysqld=ARGS           Specify additional arguments to "mysqld"

Options to run test on running server

  extern                Use running server for tests FIXME DANGEROUS
  ndb-connectstring=STR Use running cluster, and connect using STR
  ndb-connectstring-slave=STR Use running slave cluster, and connect using STR
  user=USER             User for connect to server

Options for debugging the product

  client-ddd            Start mysqltest client in ddd
  client-debugger=NAME  Start mysqltest in the selected debugger
  client-gdb            Start mysqltest client in gdb
  ddd                   Start mysqld in ddd
  debug                 Dump trace output for all servers and client programs
  debugger=NAME         Start mysqld in the selected debugger
  gdb                   Start the mysqld(s) in gdb
  manual-debug          Let user manually start mysqld in debugger, before
                        running test(s)
  manual-gdb            Let user manually start mysqld in gdb, before running
                        test(s)
  master-binary=PATH    Specify the master "mysqld" to use
  slave-binary=PATH     Specify the slave "mysqld" to use
  strace-client         Create strace output for mysqltest client

Options for coverage, profiling etc

  gcov                  FIXME
  gprof                 FIXME
  valgrind              Run the "mysqltest" and "mysqld" executables using
                        valgrind with options($default_valgrind_options)
  valgrind-all          Synonym for --valgrind
  valgrind-mysqltest    Run the "mysqltest" executable with valgrind
  valgrind-mysqld       Run the "mysqld" executable with valgrind
  valgrind-options=ARGS Options to give valgrind, replaces default options
  valgrind-path=[EXE]   Path to the valgrind executable
  callgrind             Instruct valgrind to use callgrind

Misc options

  comment=STR           Write STR to the output
  notimer               Don't show test case execution time
  script-debug          Debug this script itself
  verbose               More verbose output
  start-and-exit        Only initialize and start the servers, using the
                        startup settings for the specified test case (if any)
  start-dirty           Only start the servers (without initialization) for
                        the specified test case (if any)
  fast                  Don't try to clean up from earlier runs
  reorder               Reorder tests to get fewer server restarts
  help                  Get this help text
  unified-diff | udiff  When presenting differences, use unified diff

  testcase-timeout=MINUTES Max test case run time (default $default_testcase_timeout)
  suite-timeout=MINUTES    Max test suite run time (default $default_suite_timeout)


Deprecated options
  with-openssl          Deprecated option for ssl


Options not yet described, or that I want to look into more
  local                 
  netware               
  sleep=SECONDS         
  socket=PATH           
  user-test=s           
  wait-timeout=SECONDS  
  warnings              
  log-warnings          

HERE
  mtr_exit(1);

}
