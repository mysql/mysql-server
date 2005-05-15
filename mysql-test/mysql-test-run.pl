#!/usr/bin/perl
# -*- cperl -*-

# This is a transformation of the "mysql-test-run" Bourne shell script
# to Perl. This is just an intermediate step, the goal is to rewrite
# the Perl script to C. The complexity of the mysql-test-run script
# makes it a bit hard to write and debug it as a C program directly,
# so this is considered a prototype.
#
# Because of this the Perl coding style may in some cases look a bit
# funny. The rules used are
#
#   - The coding style is as close as possible to the C/C++ MySQL
#     coding standard.
#
#   - Where NULL is to be returned, the undefined value is used.
#
#   - Regexp comparisons are simple and can be translated to strcmp
#     and other string functions. To ease this transformation matching
#     is done in the lib "lib/mtr_match.pl", i.e. regular expressions
#     should be avoided in the main program.
#
#   - The "unless" construct is not to be used. It is the same as "if !".
#
#   - opendir/readdir/closedir is used instead of glob()/<*>.
#
#   - All lists of arguments to send to commands are Perl lists/arrays,
#     not strings we append args to. Within reason, most string
#     concatenation for arguments should be avoided.
#
#   - sprintf() is to be used, within reason, for all string creation.
#     This mtr_add_arg() function is also based on sprintf(), i.e. you
#     use a format string and put the variable argument in the argument
#     list.
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
use Cwd;
use Getopt::Long;
use Sys::Hostname;
#use Carp;
use IO::Socket;
use IO::Socket::INET;
#use Data::Dumper;
use strict;
#use diagnostics;

require "lib/mtr_cases.pl";
require "lib/mtr_process.pl";
require "lib/mtr_io.pl";
require "lib/mtr_gcov.pl";
require "lib/mtr_gprof.pl";
require "lib/mtr_report.pl";
require "lib/mtr_diff.pl";
require "lib/mtr_match.pl";
require "lib/mtr_misc.pl";

$Devel::Trace::TRACE= 1;

my @skip_if_embedded_server=
  (
   "alter_table",
   "bdb-deadlock",
   "connect",
   "flush_block_commit",
   "grant2",
   "grant_cache",
   "grant",
   "init_connect",
   "innodb-deadlock",
   "innodb-lock",
   "mix_innodb_myisam_binlog",
   "mysqlbinlog2",
   "mysqlbinlog",
   "mysqldump",
   "mysql_protocols",
   "ps_1general",
   "rename",
   "show_check",
   "system_mysql_db_fix",
   "user_var",
   "variables",
 );

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
our $glob_use_running_server=     0;
our $glob_use_running_ndbcluster= 0;
our $glob_use_embedded_server=    0;

our $glob_basedir;

# The total result

our $path_charsetsdir;
our $path_client_bindir;
our $path_language;
our $path_timefile;
our $path_manager_log;           # Used by mysqldadmin
our $path_slave_load_tmpdir;     # What is this?!
our $path_my_basedir;
our $opt_tmpdir;                 # A path but set directly on cmd line

our $opt_usage;
our $opt_suite;

our $opt_netware;

our $opt_script_debug= 0;  # Script debugging, enable with --script-debug

# Options FIXME not all....

our $exe_master_mysqld;
our $exe_mysql;
our $exe_mysqladmin;
our $exe_mysqlbinlog;
our $exe_mysql_client_test;
our $exe_mysqld;
our $exe_mysqldump;              # Called from test case
our $exe_mysql_fix_system_tables;
our $exe_mysqltest;
our $exe_slave_mysqld;

our $opt_bench= 0;
our $opt_small_bench= 0;
our $opt_big_test= 0;            # Send --big-test to mysqltest

our $opt_extra_mysqld_opt;       # FIXME not handled

our $opt_compress;
our $opt_current_test;
our $opt_ddd;
our $opt_debug;
our $opt_do_test;
our @opt_cases;                  # The test cases names in argv
our $opt_embedded_server;
our $opt_extern;
our $opt_fast;
our $opt_force;
our $opt_reorder;

our $opt_gcov;
our $opt_gcov_err;
our $opt_gcov_msg;

our $opt_gdb;
our $opt_client_gdb;
our $opt_manual_gdb;

our $opt_gprof;
our $opt_gprof_dir;
our $opt_gprof_master;
our $opt_gprof_slave;

our $opt_local;
our $opt_local_master;

our $master;                    # Will be struct in C
our $slave;

our $opt_ndbcluster_port;
our $opt_ndbconnectstring;

our $opt_no_manager;            # Does nothing now, we never use manager

our $opt_old_master;

our $opt_record;

our $opt_result_ext;

our $opt_skip;
our $opt_skip_rpl;
our $opt_skip_test;

our $opt_sleep;
our $opt_ps_protocol;

our $opt_sleep_time_after_restart= 1;
our $opt_sleep_time_for_delete=    10;

our $opt_socket;

our $opt_source_dist;

our $opt_start_and_exit;
our $opt_start_dirty;
our $opt_start_from;

our $opt_strace_client;

our $opt_timer;

our $opt_user;
our $opt_user_test;

our $opt_valgrind;
our $opt_valgrind_all;
our $opt_valgrind_options;

our $opt_verbose;

our $opt_wait_for_master;
our $opt_wait_for_slave;
our $opt_wait_timeout=  10;

our $opt_warnings;

our $opt_udiff;

our $opt_with_ndbcluster;
our $opt_with_openssl;

our $exe_ndb_mgm;
our $path_ndb_tools_dir;
our $path_ndb_backup_dir;
our $file_ndb_testrun_log;
our $flag_ndb_status_ok= 1;

######################################################################
#
#  Function declarations
#
######################################################################

sub main ();
sub initial_setup ();
sub command_line_setup ();
sub executable_setup ();
sub environment_setup ();
sub kill_running_server ();
sub kill_and_cleanup ();
sub ndbcluster_install ();
sub ndbcluster_start ();
sub ndbcluster_stop ();
sub run_benchmarks ($);
sub run_tests ();
sub mysql_install_db ();
sub install_db ($$);
sub run_testcase ($);
sub report_failure_and_restart ($);
sub do_before_start_master ($$);
sub do_before_start_slave ($$);
sub mysqld_start ($$$$);
sub mysqld_arguments ($$$$$);
sub stop_masters_slaves ();
sub stop_masters ();
sub stop_slaves ();
sub run_mysqltest ($$);
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

  if ( ! $glob_use_running_server )
  {

    if ( $opt_start_dirty )
    {
      kill_running_server();
    }
    else
    {
      kill_and_cleanup();
      mysql_install_db();

#    mysql_loadstd();  FIXME copying from "std_data" .frm and
#                      .MGR but there are none?!
    }
  }

  if ( $opt_start_and_exit or $opt_start_dirty )
  {
    if ( ndbcluster_start() )
    {
      mtr_error("Can't start ndbcluster");
    }
    if ( mysqld_start('master',0,[],[]) )
    {
      mtr_report("Servers started, exiting");
    }
    else
    {
      mtr_error("Can't start the mysqld server");
    }
  }
  else
  {
    if ( $opt_bench )
    {
      run_benchmarks(shift);      # Shift what? Extra arguments?!
    }
    else
    {
      run_tests();
    }
  }

  exit(0);
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

  $glob_hostname=  mtr_full_hostname();

  # 'basedir' is always parent of "mysql-test" directory
  $glob_mysql_test_dir=  cwd();
  if ( $glob_cygwin_perl )
  {
    # Windows programs like 'mysqld' needs Windows paths
    $glob_mysql_test_dir= `cygpath -m $glob_mysql_test_dir`;
    $glob_cygwin_shell=   `cygpath -w $ENV{'SHELL'}`; # The Windows path c:\...
    chomp($glob_mysql_test_dir);
    chomp($glob_cygwin_shell);
  }
  $glob_basedir=         dirname($glob_mysql_test_dir);
  $glob_mysql_bench_dir= "$glob_basedir/mysql-bench"; # FIXME make configurable

  $path_timefile=  "$glob_mysql_test_dir/var/log/mysqltest-time";

  # needs to be same length to test logging (FIXME what???)
  $path_slave_load_tmpdir=  "../../var/tmp";

  $path_my_basedir=
    $opt_source_dist ? $glob_mysql_test_dir : $glob_basedir;
}



##############################################################################
#
#  Default settings
#
##############################################################################

sub command_line_setup () {

  # These are defaults for things that are set on the command line

  $opt_suite=        "main";    # Special default suite
  $opt_tmpdir=       "$glob_mysql_test_dir/var/tmp";
  # FIXME maybe unneded?
  $path_manager_log= "$glob_mysql_test_dir/var/log/manager.log";
  $opt_current_test= "$glob_mysql_test_dir/var/log/current_test";

  my $opt_master_myport=   9306;
  my $opt_slave_myport=    9308;
  $opt_ndbcluster_port= 9350;

  # Read the command line
  # Note: Keep list, and the order, in sync with usage at end of this file

  GetOptions(
             # Control what engine/variation to run
             'embedded-server'          => \$opt_embedded_server,
             'ps-protocol'              => \$opt_ps_protocol,
             'bench'                    => \$opt_bench,
             'small-bench'              => \$opt_small_bench,
             'no-manager'               => \$opt_no_manager,

             # Control what test suites or cases to run
             'force'                    => \$opt_force,
             'with-ndbcluster'          => \$opt_with_ndbcluster,
             'do-test=s'                => \$opt_do_test,
             'suite=s'                  => \$opt_suite,
             'skip-rpl'                 => \$opt_skip_rpl,
             'skip-test=s'              => \$opt_skip_test,

             # Specify ports
             'master_port=i'            => \$opt_master_myport,
             'slave_port=i'             => \$opt_slave_myport,
             'ndbcluster_port=i'        => \$opt_ndbcluster_port,

             # Test case authoring
             'record'                   => \$opt_record,

             # ???
             'mysqld=s'                 => \$opt_extra_mysqld_opt,

             # Run test on running server
             'extern'                   => \$opt_extern,
             'ndbconnectstring=s'       => \$opt_ndbconnectstring,

             # Debugging
             'gdb'                      => \$opt_gdb,
             'manual-gdb'               => \$opt_manual_gdb,
             'client-gdb'               => \$opt_client_gdb,
             'ddd'                      => \$opt_ddd,
             'strace-client'            => \$opt_strace_client,
             'master-binary=s'          => \$exe_master_mysqld,
             'slave-binary=s'           => \$exe_slave_mysqld,

             # Coverage, profiling etc
             'gcov'                     => \$opt_gcov,
             'gprof'                    => \$opt_gprof,
             'valgrind'                 => \$opt_valgrind,
             'valgrind-all'             => \$opt_valgrind_all,
             'valgrind-options=s'       => \$opt_valgrind_options,

             # Misc
             'big-test'                 => \$opt_big_test,
             'compress'                 => \$opt_compress,
             'debug'                    => \$opt_debug,
             'fast'                     => \$opt_fast,
             'local'                    => \$opt_local,
             'local-master'             => \$opt_local_master,
             'netware'                  => \$opt_netware,
             'old-master'               => \$opt_old_master,
             'reorder'                  => \$opt_reorder,
             'script-debug'             => \$opt_script_debug,
             'sleep=i'                  => \$opt_sleep,
             'socket=s'                 => \$opt_socket,
             'start-dirty'              => \$opt_start_dirty,
             'start-and-exit'           => \$opt_start_and_exit,
             'start-from=s'             => \$opt_start_from,
             'timer'                    => \$opt_timer,
             'tmpdir=s'                 => \$opt_tmpdir,
             'unified-diff|udiff'       => \$opt_udiff,
             'user-test=s'              => \$opt_user_test,
             'user=s'                   => \$opt_user,
             'verbose'                  => \$opt_verbose,
             'wait-timeout=i'           => \$opt_wait_timeout,
             'warnings|log-warnings'    => \$opt_warnings,
             'with-openssl'             => \$opt_with_openssl,

             'help|h'                   => \$opt_usage,
            ) or usage("Can't read options");

  if ( $opt_usage )
  {
    usage("");
  }

  @opt_cases= @ARGV;

  # Put this into a hash, will be a C struct

  $master->[0]->{'path_myddir'}=  "$glob_mysql_test_dir/var/master-data";
  $master->[0]->{'path_myerr'}=   "$glob_mysql_test_dir/var/log/master.err";
  $master->[0]->{'path_mylog'}=   "$glob_mysql_test_dir/var/log/master.log";
  $master->[0]->{'path_mypid'}=   "$glob_mysql_test_dir/var/run/master.pid";
  $master->[0]->{'path_mysock'}=  "$opt_tmpdir/master.sock";
  $master->[0]->{'path_myport'}=   $opt_master_myport;
  $master->[0]->{'start_timeout'}= 400; # enough time create innodb tables

  $master->[0]->{'ndbcluster'}= 1; # ndbcluster not started

  $master->[1]->{'path_myddir'}=  "$glob_mysql_test_dir/var/master1-data";
  $master->[1]->{'path_myerr'}=   "$glob_mysql_test_dir/var/log/master1.err";
  $master->[1]->{'path_mylog'}=   "$glob_mysql_test_dir/var/log/master1.log";
  $master->[1]->{'path_mypid'}=   "$glob_mysql_test_dir/var/run/master1.pid";
  $master->[1]->{'path_mysock'}=  "$opt_tmpdir/master1.sock";
  $master->[1]->{'path_myport'}=   $opt_master_myport + 1;
  $master->[1]->{'start_timeout'}= 400; # enough time create innodb tables

  $slave->[0]->{'path_myddir'}=   "$glob_mysql_test_dir/var/slave-data";
  $slave->[0]->{'path_myerr'}=    "$glob_mysql_test_dir/var/log/slave.err";
  $slave->[0]->{'path_mylog'}=    "$glob_mysql_test_dir/var/log/slave.log";
  $slave->[0]->{'path_mypid'}=    "$glob_mysql_test_dir/var/run/slave.pid";
  $slave->[0]->{'path_mysock'}=   "$opt_tmpdir/slave.sock";
  $slave->[0]->{'path_myport'}=    $opt_slave_myport;
  $slave->[0]->{'start_timeout'}=  400;

  $slave->[1]->{'path_myddir'}=   "$glob_mysql_test_dir/var/slave1-data";
  $slave->[1]->{'path_myerr'}=    "$glob_mysql_test_dir/var/log/slave1.err";
  $slave->[1]->{'path_mylog'}=    "$glob_mysql_test_dir/var/log/slave1.log";
  $slave->[1]->{'path_mypid'}=    "$glob_mysql_test_dir/var/run/slave1.pid";
  $slave->[1]->{'path_mysock'}=   "$opt_tmpdir/slave1.sock";
  $slave->[1]->{'path_myport'}=    $opt_slave_myport + 1;
  $slave->[1]->{'start_timeout'}=  300;

  $slave->[2]->{'path_myddir'}=   "$glob_mysql_test_dir/var/slave2-data";
  $slave->[2]->{'path_myerr'}=    "$glob_mysql_test_dir/var/log/slave2.err";
  $slave->[2]->{'path_mylog'}=    "$glob_mysql_test_dir/var/log/slave2.log";
  $slave->[2]->{'path_mypid'}=    "$glob_mysql_test_dir/var/run/slave2.pid";
  $slave->[2]->{'path_mysock'}=   "$opt_tmpdir/slave2.sock";
  $slave->[2]->{'path_myport'}=    $opt_slave_myport + 2;
  $slave->[2]->{'start_timeout'}=  300;

  # Do sanity checks of command line arguments

  if ( $opt_extern and $opt_local )
  {
    mtr_error("Can't use --extern and --local at the same time");
  }

  if ( ! $opt_socket )
  {     # FIXME set default before reading options?
#    $opt_socket=  '@MYSQL_UNIX_ADDR@';
    $opt_socket=  "/tmp/mysql.sock"; # FIXME
  }

  if ( $opt_extern )
  {
    $glob_use_running_server=  1;
    $opt_skip_rpl= 1;                   # We don't run rpl test cases
    $master->[0]->{'path_mysock'}=  $opt_socket;
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
    $opt_skip_rpl= 1;              # We never run replication with embedded

    if ( $opt_extern )
    {
      mtr_error("Can't use --extern with --embedded-server");
    }
  }

  # FIXME don't understand what this is
#  if ( $opt_local_master )
#  {
#    $opt_master_myport=  3306;
#  }

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

  if ( $glob_use_embedded_server and ! $opt_source_dist )
  {
    mtr_error("Embedded server needs source tree - please use source dist");
  }

  if ( $opt_gdb )
  {
    $opt_wait_timeout=  300;
    if ( $opt_extern )
    {
      mtr_error("Can't use --extern with --gdb");
    }
  }

  if ( $opt_manual_gdb )
  {
    $opt_gdb=  1;
    if ( $opt_extern )
    {
      mtr_error("Can't use --extern with --manual-gdb");
    }
  }

  if ( $opt_ddd )
  {
    if ( $opt_extern )
    {
      mtr_error("Can't use --extern with --ddd");
    }
  }

  if ( $opt_ndbconnectstring )
  {
    $glob_use_running_ndbcluster= 1;
    $opt_with_ndbcluster= 1;
  }
  else
  {
    $opt_ndbconnectstring= "host=localhost:$opt_ndbcluster_port";
  }

  # FIXME

  #if ( $opt_valgrind or $opt_valgrind_all )
  #{
    # VALGRIND=`which valgrind` # this will print an error if not found FIXME
    # Give good warning to the user and stop
  #  if ( ! $VALGRIND )
  #  {
  #    print "You need to have the 'valgrind' program in your PATH to run mysql-test-run with option --valgrind. Valgrind's home page is http://valgrind.kde.org.\n"
  #    exit 1
  #  }
    # >=2.1.2 requires the --tool option, some versions write to stdout, some to stderr
  #  valgrind --help 2>&1 | grep "\-\-tool" > /dev/null && VALGRIND="$VALGRIND --tool=memcheck"
  #  VALGRIND="$VALGRIND --alignment=8 --leak-check=yes --num-callers=16"
  #  $opt_extra_mysqld_opt.= " --skip-safemalloc --skip-bdb";
  #  SLEEP_TIME_AFTER_RESTART=10
  #  $opt_sleep_time_for_delete=  60
  #  $glob_use_running_server= ""
  #  if ( "$1"=  "--valgrind-all" )
  #  {
  #    VALGRIND="$VALGRIND -v --show-reachable=yes"
  #  }
  #}

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

}


##############################################################################
#
#  Set paths to various executable programs
#
##############################################################################

sub executable_setup () {

  if ( $opt_source_dist )
  {
    if ( $glob_use_embedded_server )
    {
      if ( -f "$glob_basedir/libmysqld/examples/mysqltest" )
      {
        $exe_mysqltest=  "$glob_basedir/libmysqld/examples/mysqltest";
      }
      else
      {
        mtr_error("Can't find embedded server 'mysqltest'");
      }
      $exe_mysql_client_test=
        "$glob_basedir/libmysqld/examples/mysql_client_test_embedded";
    }
    else
    {
      if ( -f "$glob_basedir/client/.libs/lt-mysqltest" )
      {
        $exe_mysqltest=  "$glob_basedir/client/.libs/lt-mysqltest";
      }
      elsif ( -f "$glob_basedir/client/.libs/mysqltest" )
      {
        $exe_mysqltest=  "$glob_basedir/client/.libs/mysqltest";
      }
      else
      {
        $exe_mysqltest=  "$glob_basedir/client/mysqltest";
      }
      $exe_mysql_client_test=
        "$glob_basedir/tests/mysql_client_test";
    }
    if ( -f "$glob_basedir/client/.libs/mysqldump" )
    {
      $exe_mysqldump=  "$glob_basedir/client/.libs/mysqldump";
    }
    else
    {
      $exe_mysqldump=  "$glob_basedir/client/mysqldump";
    }
    if ( -f "$glob_basedir/client/.libs/mysqlbinlog" )
    {
      $exe_mysqlbinlog=  "$glob_basedir/client/.libs/mysqlbinlog";
    }
    else
    {
      $exe_mysqlbinlog=   "$glob_basedir/client/mysqlbinlog";
    }

    $path_client_bindir=  "$glob_basedir/client";
    $exe_mysqld=          "$glob_basedir/sql/mysqld";
    $exe_mysqladmin=      "$path_client_bindir/mysqladmin";
    $exe_mysql=           "$path_client_bindir/mysql";
    $exe_mysql_fix_system_tables= "$glob_basedir/scripts/mysql_fix_privilege_tables";
    $path_language=       "$glob_basedir/sql/share/english/";
    $path_charsetsdir=    "$glob_basedir/sql/share/charsets";

    $path_ndb_tools_dir=  "$glob_basedir/ndb/tools";
    $exe_ndb_mgm=         "$glob_basedir/ndb/src/mgmclient/ndb_mgm";
  }
  else
  {
    my $path_tests_bindir=  "$glob_basedir/tests";

    $path_client_bindir=    "$glob_basedir/bin";
    $exe_mysqltest=         "$path_client_bindir/mysqltest";
    $exe_mysqldump=         "$path_client_bindir/mysqldump";
    $exe_mysqlbinlog=       "$path_client_bindir/mysqlbinlog";
    $exe_mysqladmin=        "$path_client_bindir/mysqladmin";
    $exe_mysql=             "$path_client_bindir/mysql";
    $exe_mysql_fix_system_tables= "$path_client_bindir/scripts/mysql_fix_privilege_tables";

    if ( -d "$glob_basedir/share/mysql/english" )
    {
      $path_language    ="$glob_basedir/share/mysql/english/";
      $path_charsetsdir ="$glob_basedir/share/mysql/charsets";
    }
    else
    {
      $path_language    ="$glob_basedir/share/english/";
      $path_charsetsdir ="$glob_basedir/share/charsets";
    }

    if ( -x "$glob_basedir/libexec/mysqld" )
    {
      $exe_mysqld= "$glob_basedir/libexec/mysqld";
    }
    else
    {
      $exe_mysqld= "$glob_basedir/bin/mysqld";
    }

    if ( $glob_use_embedded_server )
    {
      if ( -f "$path_client_bindir/mysqltest_embedded" )
      {
        # FIXME valgrind?
        $exe_mysqltest="$path_client_bindir/mysqltest_embedded";
      }
      else
      {
        mtr_error("Cannot find embedded server 'mysqltest_embedded'");
      }
      if ( -d "$path_tests_bindir/mysql_client_test_embedded" )
      {
        $exe_mysql_client_test=
          "$path_tests_bindir/mysql_client_test_embedded";
      }
      else
      {
        $exe_mysql_client_test=
          "$path_client_bindir/mysql_client_test_embedded";
      }
    }
    else
    {
      $exe_mysqltest="$path_client_bindir/mysqltest";
      $exe_mysql_client_test="$path_client_bindir/mysql_client_test";
    }

    $path_ndb_tools_dir=  "$glob_basedir/bin";
    $exe_ndb_mgm=         "$glob_basedir/bin/ndb_mgm";
  }

  if ( ! $exe_master_mysqld )
  {
    $exe_master_mysqld=  $exe_mysqld;
  }

  if ( ! $exe_slave_mysqld )
  {
    $exe_slave_mysqld=  $exe_mysqld;
  }

  $path_ndb_backup_dir=
    "$glob_mysql_test_dir/var/ndbcluster-$opt_ndbcluster_port";
  $file_ndb_testrun_log= "$glob_mysql_test_dir/var/log/ndb_testrun.log";
}


##############################################################################
#
#  Set environment to be used by childs of this process
#
##############################################################################

# Note that some env is setup in spawn/run, in "mtr_process.pl"

sub environment_setup () {

  # --------------------------------------------------------------------------
  # Set LD_LIBRARY_PATH if we are using shared libraries
  # --------------------------------------------------------------------------

  $ENV{'LD_LIBRARY_PATH'}=
    "$glob_basedir/lib:$glob_basedir/libmysql/.libs" .
      ($ENV{'LD_LIBRARY_PATH'} ? ":$ENV{'LD_LIBRARY_PATH'}" : "");
  $ENV{'DYLD_LIBRARY_PATH'}=
    "$glob_basedir/lib:$glob_basedir/libmysql/.libs" .
      ($ENV{'DYLD_LIBRARY_PATH'} ? ":$ENV{'DYLD_LIBRARY_PATH'}" : "");

  # --------------------------------------------------------------------------
  # Also command lines in .opt files may contain env vars
  # --------------------------------------------------------------------------

  $ENV{'UMASK'}=              "0660"; # The octal *string*
  $ENV{'UMASK_DIR'}=          "0770"; # The octal *string*
  $ENV{'LC_COLLATE'}=         "C";
  $ENV{'USE_RUNNING_SERVER'}= $glob_use_running_server;
  $ENV{'MYSQL_TEST_DIR'}=     $glob_mysql_test_dir;
  $ENV{'MASTER_MYSOCK'}=      $master->[0]->{'path_mysock'};
  $ENV{'MASTER_MYSOCK1'}=     $master->[1]->{'path_mysock'};
  $ENV{'MASTER_MYPORT'}=      $master->[0]->{'path_myport'};
  $ENV{'MASTER_MYPORT1'}=     $master->[1]->{'path_myport'};
  $ENV{'SLAVE_MYPORT'}=       $slave->[0]->{'path_myport'};
# $ENV{'MYSQL_TCP_PORT'}=     '@MYSQL_TCP_PORT@'; # FIXME
  $ENV{'MYSQL_TCP_PORT'}=     3306;
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
  stop_masters_slaves();
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
    unlink($master->[0]->{'path_mypid'});
    unlink($master->[1]->{'path_mypid'});
    unlink($slave->[0]->{'path_mypid'});
    unlink($slave->[1]->{'path_mypid'});
    unlink($slave->[2]->{'path_mypid'});
  }
  else
  {
    # Ensure that no old mysqld test servers are running
    # This is different from terminating processes we have
    # started from ths run of the script, this is terminating
    # leftovers from previous runs.

    mtr_report("Killing Possible Leftover Processes");
    mkpath("$glob_mysql_test_dir/var/log"); # Needed for mysqladmin log
    mtr_kill_leftovers();

    ndbcluster_stop();
    $master->[0]->{'ndbcluster'}= 1;
  }
}

sub kill_and_cleanup () {

  kill_running_server ();

  mtr_report("Removing Stale Files");

  rmtree("$glob_mysql_test_dir/var/log");
  rmtree("$glob_mysql_test_dir/var/ndbcluster-$opt_ndbcluster_port");
  rmtree("$glob_mysql_test_dir/var/run");
  rmtree("$glob_mysql_test_dir/var/tmp");

  mkpath("$glob_mysql_test_dir/var/log");
  mkpath("$glob_mysql_test_dir/var/run");
  mkpath("$glob_mysql_test_dir/var/tmp");
  mkpath($opt_tmpdir);

  # FIXME do we really need to create these all, or are they
  # created for us when tables are created?

  rmtree("$master->[0]->{'path_myddir'}");
  mkpath("$master->[0]->{'path_myddir'}/mysql");
  mkpath("$master->[0]->{'path_myddir'}/test");

  rmtree("$master->[1]->{'path_myddir'}");
  mkpath("$master->[1]->{'path_myddir'}/mysql");
  mkpath("$master->[1]->{'path_myddir'}/test");

  rmtree("$slave->[0]->{'path_myddir'}");
  mkpath("$slave->[0]->{'path_myddir'}/mysql");
  mkpath("$slave->[0]->{'path_myddir'}/test");

  rmtree("$slave->[1]->{'path_myddir'}");
  mkpath("$slave->[1]->{'path_myddir'}/mysql");
  mkpath("$slave->[1]->{'path_myddir'}/test");

  rmtree("$slave->[2]->{'path_myddir'}");
  mkpath("$slave->[2]->{'path_myddir'}/mysql");
  mkpath("$slave->[2]->{'path_myddir'}/test");
}


##############################################################################
#
#  Start the ndb cluster
#
##############################################################################

# FIXME why is there a different start below?!

sub ndbcluster_install () {

  if ( ! $opt_with_ndbcluster or $glob_use_running_ndbcluster )
  {
    return 0;
  }
  mtr_report("Install ndbcluster");
  my $ndbcluster_opts=  $opt_bench ? "" : "--small";
  my $ndbcluster_port_base= $opt_ndbcluster_port + 2;
  if (  mtr_run("$glob_mysql_test_dir/ndb/ndbcluster",
		["--port=$opt_ndbcluster_port",
		 "--port-base=$ndbcluster_port_base",
		 "--data-dir=$glob_mysql_test_dir/var",
		 $ndbcluster_opts,
		 "--initial"],
		"", "", "", "") )
  {
    mtr_error("Error ndbcluster_install");
    return 1;
  }

  ndbcluster_stop();
  $master->[0]->{'ndbcluster'}= 1;

  return 0;
}

sub ndbcluster_start () {

  if ( ! $opt_with_ndbcluster or $glob_use_running_ndbcluster )
  {
    return 0;
  }
  # FIXME, we want to _append_ output to file $file_ndb_testrun_log instead of /dev/null
  if ( mtr_run("$glob_mysql_test_dir/ndb/ndbcluster",
	       ["--port=$opt_ndbcluster_port",
		"--data-dir=$glob_mysql_test_dir/var"],
	       "", "/dev/null", "", "") )
  {
    mtr_error("Error ndbcluster_start");
    return 1;
  }

  return 0;
}

sub ndbcluster_stop () {

  if ( ! $opt_with_ndbcluster or $glob_use_running_ndbcluster )
  {
    return;
  }
  my $ndbcluster_port_base= $opt_ndbcluster_port + 2;
  # FIXME, we want to _append_ output to file $file_ndb_testrun_log instead of /dev/null
  mtr_run("$glob_mysql_test_dir/ndb/ndbcluster",
          ["--port=$opt_ndbcluster_port",
           "--data-dir=$glob_mysql_test_dir/var",
           "--stop"],
          "", "/dev/null", "", "");

  return;
}


##############################################################################
#
#  Run the benchmark suite
#
##############################################################################

sub run_benchmarks ($) {
  my $benchmark=  shift;

  my $args;

  if ( ! $glob_use_embedded_server and ! $opt_local_master )
  {
    $master->[0]->{'pid'}= mysqld_start('master',0,[],[]);
    if ( ! $master->[0]->{'pid'} )
    {
      mtr_error("Can't start the mysqld server");
    }
  }

  mtr_init_args(\$args);

  mtr_add_arg($args, "--socket=%s", $master->[0]->{'path_mysock'});
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

  my $benchdir=  "$glob_basedir/sql-bench";
  chdir($benchdir);             # FIXME check error

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

sub run_tests () {
  run_suite($opt_suite);
}

sub run_suite () {
  my $suite= shift;

  mtr_print_thick_line();

  mtr_report("Finding  Tests in the '$suite' suite");

  my $tests= collect_test_cases($suite);

  mtr_report("Starting Tests in the '$suite' suite");

  mtr_print_header();

  foreach my $tinfo ( @$tests )
  {
    run_testcase($tinfo);
  }

  mtr_print_line();

  if ( ! $opt_gdb and ! $glob_use_running_server and
       ! $opt_ddd and ! $glob_use_embedded_server )
  {
    stop_masters_slaves();
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
}


##############################################################################
#
#  Initiate the test databases
#
##############################################################################

sub mysql_install_db () {

  # FIXME not exactly true I think, needs improvements
  install_db('master', $master->[0]->{'path_myddir'});
  install_db('master', $master->[1]->{'path_myddir'});
  install_db('slave',  $slave->[0]->{'path_myddir'});
  install_db('slave',  $slave->[1]->{'path_myddir'});
  install_db('slave',  $slave->[2]->{'path_myddir'});

  if ( ndbcluster_install() )
  {
    # failed to install, disable usage but flag that its no ok
    $opt_with_ndbcluster= 0;
    $flag_ndb_status_ok= 0;
  }

  return 0;
}


sub install_db ($$) {
  my $type=      shift;
  my $data_dir=  shift;

  my $init_db_sql=     "lib/init_db.sql";
  my $init_db_sql_tmp= "/tmp/init_db.sql$$";
  my $args;

  mtr_report("Installing \u$type Databases");

  open(IN, $init_db_sql)
    or error("Can't open $init_db_sql: $!");
  open(OUT, ">", $init_db_sql_tmp)
    or error("Can't write to $init_db_sql_tmp: $!");
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
  mtr_add_arg($args, "--skip-bdb");

  if ( ! $opt_netware )
  {
    mtr_add_arg($args, "--language=%s", $path_language);
    mtr_add_arg($args, "--character-sets-dir=%s", $path_charsetsdir);
  }

  if ( mtr_run($exe_mysqld, $args, $init_db_sql_tmp,
               $path_manager_log, $path_manager_log, "") != 0 )
  {
    unlink($init_db_sql_tmp);
    mtr_error("Error executing mysqld --bootstrap\n" .
              "Could not install $type test DBs");
  }
  unlink($init_db_sql_tmp);
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

  my $tname= $tinfo->{'name'};

  mtr_tonewfile($opt_current_test,"$tname\n"); # Always tell where we are

  # output current test to ndbcluster log file to enable diagnostics
  mtr_tofile($file_ndb_testrun_log,"CURRENT TEST $tname\n");

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
    return;
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

  if ( ! $glob_use_running_server and ! $glob_use_embedded_server )
  {
    if ( $tinfo->{'master_restart'} or $master->[0]->{'uses_special_flags'} )
    {
      stop_masters();
      $master->[0]->{'uses_special_flags'}= 0; # Forget about why we stopped
    }

    # ----------------------------------------------------------------------
    # Always terminate all slaves, if any. Else we may have useless
    # reconnection attempts and error messages in case the slave and
    # master servers restart.
    # ----------------------------------------------------------------------

    stop_slaves();
  }    

  # ----------------------------------------------------------------------
  # Prepare to start masters. Even if we use embedded, we want to run
  # the preparation.
  # ----------------------------------------------------------------------

  $ENV{'TZ'}= $tinfo->{'timezone'};

  mtr_report_test_name($tinfo);

  mtr_tofile($master->[0]->{'path_myerr'},"CURRENT_TEST: $tname\n");

# FIXME test cases that depend on each other, prevent this from
# being at this location.
#  do_before_start_master($tname,$tinfo->{'master_sh'});

  # ----------------------------------------------------------------------
  # If any mysqld servers running died, we have to know
  # ----------------------------------------------------------------------

  mtr_record_dead_children();

  # ----------------------------------------------------------------------
  # Start masters
  # ----------------------------------------------------------------------

  if ( ! $glob_use_running_server and ! $glob_use_embedded_server )
  {
    # FIXME give the args to the embedded server?!
    # FIXME what does $opt_local_master mean?!
    # FIXME split up start and check that started so that can do
    #       starts in parallel, masters and slaves at the same time.

    if ( ! $opt_local_master )
    {
      if ( $master->[0]->{'ndbcluster'} )
      {
	$master->[0]->{'ndbcluster'}= ndbcluster_start();
        if ( $master->[0]->{'ndbcluster'} )
        {
          report_failure_and_restart($tinfo);
          return;
        }
      }
      if ( ! $master->[0]->{'pid'} )
      {
        # FIXME not correct location for do_before_start_master()
        do_before_start_master($tname,$tinfo->{'master_sh'});
        $master->[0]->{'pid'}=
          mysqld_start('master',0,$tinfo->{'master_opt'},[]);
        if ( ! $master->[0]->{'pid'} )
        {
          report_failure_and_restart($tinfo);
          return;
        }
      }
      if ( $opt_with_ndbcluster and ! $master->[1]->{'pid'} )
      {
        $master->[1]->{'pid'}=
          mysqld_start('master',1,$tinfo->{'master_opt'},[]);
        if ( ! $master->[1]->{'pid'} )
        {
          report_failure_and_restart($tinfo);
          return;
        }
      }

      if ( @{$tinfo->{'master_opt'}} )
      {
        $master->[0]->{'uses_special_flags'}= 1;
      }
    }

    # ----------------------------------------------------------------------
    # Start slaves - if needed
    # ----------------------------------------------------------------------

    if ( $tinfo->{'slave_num'} )
    {
      mtr_tofile($slave->[0]->{'path_myerr'},"CURRENT_TEST: $tname\n");

      do_before_start_slave($tname,$tinfo->{'slave_sh'});

      for ( my $idx= 0; $idx <  $tinfo->{'slave_num'}; $idx++ )
      {
        if ( ! $slave->[$idx]->{'pid'} )
        {
          $slave->[$idx]->{'pid'}=
            mysqld_start('slave',$idx,
                         $tinfo->{'slave_opt'}, $tinfo->{'slave_mi'});
          if ( ! $slave->[$idx]->{'pid'} )
          {
            report_failure_and_restart($tinfo);
            return;
          }
        }
      }
    }
  }

  # ----------------------------------------------------------------------
  # Run the test case
  # ----------------------------------------------------------------------

  {
    # remove the old reject file
    if ( $opt_suite eq "main" )
    {
      unlink("r/$tname.reject");
    }
    else
    {
      unlink("suite/$opt_suite/r/$tname.reject");
    }
    unlink($path_timefile);

    my $res= run_mysqltest($tinfo, $tinfo->{'master_opt'});

    if ( $res == 0 )
    {
      mtr_report_test_passed($tinfo);
    }
    elsif ( $res == 62 )
    {
      # Testcase itself tell us to skip this one
      mtr_report_test_skipped($tinfo);
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
  }
}


sub report_failure_and_restart ($) {
  my $tinfo= shift;

  mtr_report_test_failed($tinfo);
  mtr_show_failed_diff($tinfo->{'name'});
  print "\n";
  if ( ! $opt_force )
  {
    print "Aborting: $tinfo->{'name'} failed. To continue, re-run with '--force'.";
    print "\n";
    if ( ! $opt_gdb and ! $glob_use_running_server and
         ! $opt_ddd and ! $glob_use_embedded_server )
    {
      stop_masters_slaves();
    }
    exit(1);
  }

  # FIXME always terminate on failure?!
  if ( ! $opt_gdb and ! $glob_use_running_server and
       ! $opt_ddd and ! $glob_use_embedded_server )
  {
    stop_masters_slaves();
  }
  print "Resuming Tests\n\n";
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
    foreach my $bin ( glob("$glob_mysql_test_dir/var/log/master*-bin.*") )
    {
      unlink($bin);
    }
  }

  # Remove old master.info and relay-log.info files
  unlink("$glob_mysql_test_dir/var/master-data/master.info");
  unlink("$glob_mysql_test_dir/var/master-data/relay-log.info");
  unlink("$glob_mysql_test_dir/var/master1-data/master.info");
  unlink("$glob_mysql_test_dir/var/master1-data/relay-log.info");

  # Run master initialization shell script if one exists
  if ( $init_script )
  {
    my $ret= mtr_run("/bin/sh", [$init_script], "", "", "", "");
    if ( $ret != 0 )
    {
      # FIXME rewrite those scripts to return 0 if successful
#      mtr_warning("$init_script exited with code $ret");
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
    foreach my $bin ( glob("$glob_mysql_test_dir/var/log/slave*-bin.*") )
    {
      unlink($bin);
    }
    # FIXME really master?!
    unlink("$glob_mysql_test_dir/var/slave-data/master.info");
    unlink("$glob_mysql_test_dir/var/slave-data/relay-log.info");
  }

  # Run slave initialization shell script if one exists
  if ( $init_script )
  {
    my $ret= mtr_run("/bin/sh", [$init_script], "", "", "", "");
    if ( $ret != 0 )
    {
      # FIXME rewrite those scripts to return 0 if successful
#      mtr_warning("$init_script exited with code $ret");
    }
  }

  `rm -f $glob_mysql_test_dir/var/slave-data/log.*`;
#  unlink("$glob_mysql_test_dir/var/slave-data/log.*");
}

sub mysqld_arguments ($$$$$) {
  my $args=              shift;
  my $type=              shift;        # master/slave/bootstrap
  my $idx=               shift;
  my $extra_opt=         shift;
  my $slave_master_info= shift;

#  print STDERR Dumper($extra_opt);

  my $sidx= "";                 # Index as string, 0 is empty string
  if ( $idx > 0 )
  {
    $sidx= sprintf("%d", $idx); # sprintf not needed in Perl for this
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
  mtr_add_arg($args, "%s--default-character-set=latin1", $prefix);
  mtr_add_arg($args, "%s--language=%s", $prefix, $path_language);
  mtr_add_arg($args, "%s--tmpdir=$opt_tmpdir", $prefix);

  if ( $opt_valgrind )
  {
    mtr_add_arg($args, "%s--skip-safemalloc", $prefix);
    mtr_add_arg($args, "%s--skip-bdb", $prefix);
  }

  my $pidfile;

  if ( $type eq 'master' )
  {
    mtr_add_arg($args, "%s--log-bin=%s/var/log/master-bin", $prefix,
                $glob_mysql_test_dir);
    mtr_add_arg($args, "%s--pid-file=%s", $prefix,
                $master->[$idx]->{'path_mypid'});
    mtr_add_arg($args, "%s--port=%d", $prefix,
                $master->[$idx]->{'path_myport'});
    mtr_add_arg($args, "%s--server-id=1", $prefix);
    mtr_add_arg($args, "%s--socket=%s", $prefix,
                $master->[$idx]->{'path_mysock'});
    mtr_add_arg($args, "%s--innodb_data_file_path=ibdata1:50M", $prefix);
    mtr_add_arg($args, "%s--local-infile", $prefix);
    mtr_add_arg($args, "%s--datadir=%s", $prefix,
                $master->[$idx]->{'path_myddir'});
  }

  if ( $type eq 'slave' )
  {
    my $slave_server_id=  2 + $idx;
    my $slave_rpl_rank= $idx > 0 ? 2 : $slave_server_id;

    mtr_add_arg($args, "%s--datadir=%s", $prefix,
                $slave->[$idx]->{'path_myddir'});
    # FIXME slave get this option twice?!
    mtr_add_arg($args, "%s--exit-info=256", $prefix);
    mtr_add_arg($args, "%s--init-rpl-role=slave", $prefix);
    mtr_add_arg($args, "%s--log-bin=%s/var/log/slave%s-bin", $prefix,
                $glob_mysql_test_dir, $sidx); # FIXME use own dir for binlogs
    mtr_add_arg($args, "%s--log-slave-updates", $prefix);
    # FIXME option duplicated for slave
    mtr_add_arg($args, "%s--log=%s", $prefix,
                $slave->[$idx]->{'path_mylog'});
    mtr_add_arg($args, "%s--master-retry-count=10", $prefix);
    mtr_add_arg($args, "%s--pid-file=%s", $prefix,
                $slave->[$idx]->{'path_mypid'});
    mtr_add_arg($args, "%s--port=%d", $prefix,
                $slave->[$idx]->{'path_myport'});
    mtr_add_arg($args, "%s--relay-log=%s/var/log/slave%s-relay-bin", $prefix,
                $glob_mysql_test_dir, $sidx);
    mtr_add_arg($args, "%s--report-host=127.0.0.1", $prefix);
    mtr_add_arg($args, "%s--report-port=%d", $prefix,
                $slave->[$idx]->{'path_myport'});
    mtr_add_arg($args, "%s--report-user=root", $prefix);
    mtr_add_arg($args, "%s--skip-innodb", $prefix);
    mtr_add_arg($args, "%s--skip-ndbcluster", $prefix);
    mtr_add_arg($args, "%s--skip-slave-start", $prefix);
    mtr_add_arg($args, "%s--slave-load-tmpdir=%s", $prefix,
                $path_slave_load_tmpdir);
    mtr_add_arg($args, "%s--socket=%s", $prefix,
                $slave->[$idx]->{'path_mysock'});
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
                  $master->[0]->{'path_myport'}); # First master
      mtr_add_arg($args, "%s--server-id=%d", $prefix, $slave_server_id);
      mtr_add_arg($args, "%s--rpl-recovery-rank=%d", $prefix, $slave_rpl_rank);
    }
  } # end slave

  if ( $opt_debug )
  {
    if ( $type eq 'master' )
    {
      mtr_add_arg($args, "%s--debug=d:t:i:A,%s/var/log/master%s.trace",
                  $prefix, $glob_mysql_test_dir, $sidx);
    }
    if ( $type eq 'slave' )
    {
      mtr_add_arg($args, "%s--debug=d:t:i:A,%s/var/log/slave%s.trace",
                  $prefix, $glob_mysql_test_dir, $sidx);
    }
  }

  if ( $opt_with_ndbcluster )
  {
    mtr_add_arg($args, "%s--ndbcluster", $prefix);
    mtr_add_arg($args, "%s--ndb-connectstring=%s", $prefix,
                $opt_ndbconnectstring);
  }

  # FIXME always set nowdays??? SMALL_SERVER
  mtr_add_arg($args, "%s--key_buffer_size=1M", $prefix);
  mtr_add_arg($args, "%s--sort_buffer=256K", $prefix);
  mtr_add_arg($args, "%s--max_heap_table_size=1M", $prefix);

  if ( $opt_with_openssl )
  {
    mtr_add_arg($args, "%s--ssl-ca=%s/SSL/cacert.pem", $prefix, $glob_basedir);
    mtr_add_arg($args, "%s--ssl-cert=%s/SSL/server-cert.pem", $prefix,
                $glob_basedir);
    mtr_add_arg($args, "%s--ssl-key=%s/SSL/server-key.pem", $prefix,
                $glob_basedir);
  }

  if ( $opt_warnings )
  {
    mtr_add_arg($args, "%s--log-warnings", $prefix);
  }

  if ( $opt_gdb or $opt_client_gdb or $opt_manual_gdb or $opt_ddd)
  {
    mtr_add_arg($args, "%s--gdb", $prefix);
  }

  # If we should run all tests cases, we will use a local server for that

  if ( -w "/" )
  {
    # We are running as root;  We need to add the --root argument
    mtr_add_arg($args, "%s--user=root", $prefix);
  }

  if ( $type eq 'master' )
  {

    if ( ! $opt_old_master )
    {
      mtr_add_arg($args, "%s--rpl-recovery-rank=1", $prefix);
      mtr_add_arg($args, "%s--init-rpl-role=master", $prefix);
    }

    # FIXME strange,.....
    # FIXME MYSQL_MYPORT is not set anythere?!
    if ( $opt_local_master )
    {
      mtr_add_arg($args, "%s--host=127.0.0.1", $prefix);
      mtr_add_arg($args, "%s--port=%s", $prefix, $ENV{'MYSQL_MYPORT'});
    }
  }

  foreach my $arg ( @$extra_opt )
  {
    mtr_add_arg($args, "%s%s", $prefix, $arg);
  }

  if ( $opt_bench )
  {
    mtr_add_arg($args, "%s--rpl-recovery-rank=1", $prefix);
    mtr_add_arg($args, "%s--init-rpl-role=master", $prefix);
  }
  else
  {
    mtr_add_arg($args, "%s--exit-info=256", $prefix);
    mtr_add_arg($args, "%s--open-files-limit=1024", $prefix);

    if ( $type eq 'master' )
    {
      mtr_add_arg($args, "%s--log=%s", $prefix, $master->[0]->{'path_mylog'});
    }
    if ( $type eq 'slave' )
    {
      mtr_add_arg($args, "%s--log=%s", $prefix, $slave->[0]->{'path_mylog'});
    }
  }

  return $args;
}

# FIXME
#  if ( $type eq 'master' and $glob_use_embedded_server )
#  {
#    # Add a -A to each argument to pass it to embedded server
#    my @mysqltest_opt=  map {("-A",$_)} @args;
#    $opt_extra_mysqltest_opt=  \@mysqltest_opt;
#    return;
#  }

##############################################################################
#
#  Start mysqld and return the PID
#
##############################################################################

sub mysqld_start ($$$$) {
  my $type=              shift;        # master/slave/bootstrap
  my $idx=               shift;
  my $extra_opt=         shift;
  my $slave_master_info= shift;

  my $args;                             # Arg vector
  my $exe;
  my $pid;

  if ( $type eq 'master' )
  {
    $exe= $exe_master_mysqld;
  }
  elsif ( $type eq 'slave' )
  {
    $exe= $exe_slave_mysqld;
  }
  else
  {
    $exe= $exe_mysqld;
  }

  mtr_init_args(\$args);

  if ( $opt_valgrind )
  {

    mtr_add_arg($args, "--tool=memcheck");
    mtr_add_arg($args, "--alignment=8");
    mtr_add_arg($args, "--leak-check=yes");
    mtr_add_arg($args, "--num-callers=16");

    if ( $opt_valgrind_all )
    {
      mtr_add_arg($args, "-v");
      mtr_add_arg($args, "--show-reachable=yes");
    }

    if ( $opt_valgrind_options )
    {
      # FIXME split earlier and put into @glob_valgrind_*
      mtr_add_arg($args, split(' ', $opt_valgrind_options));
    }

    mtr_add_arg($args, $exe);

    $exe=  $opt_valgrind;
  }

  mysqld_arguments($args,$type,$idx,$extra_opt,$slave_master_info);

  if ( $type eq 'master' )
  {
    if ( $pid= mtr_spawn($exe, $args, "",
                         $master->[$idx]->{'path_myerr'},
                         $master->[$idx]->{'path_myerr'}, "") )
    {
      return sleep_until_file_created($master->[$idx]->{'path_mypid'},
                                      $master->[$idx]->{'start_timeout'}, $pid);
    }
  }

  if ( $type eq 'slave' )
  {
    if ( $pid= mtr_spawn($exe, $args, "",
                         $slave->[$idx]->{'path_myerr'},
                         $slave->[$idx]->{'path_myerr'}, "") )
    {
      return sleep_until_file_created($slave->[$idx]->{'path_mypid'},
                                      $master->[$idx]->{'start_timeout'}, $pid);
    }
  }

  return 0;
}

sub stop_masters_slaves () {

  print  "Ending Tests\n";
  print  "Shutting-down MySQL daemon\n\n";
  stop_masters();
  print "Master(s) shutdown finished\n";
  stop_slaves();
  print "Slave(s) shutdown finished\n";
}

sub stop_masters () {

  my @args;

  for ( my $idx; $idx < 2; $idx++ )
  {
    # FIXME if we hit ^C before fully started, this test will prevent
    # the mysqld process from being killed
    if ( $master->[$idx]->{'pid'} )
    {
      push(@args,{
                  pid      => $master->[$idx]->{'pid'},
                  pidfile  => $master->[$idx]->{'path_mypid'},
                  sockfile => $master->[$idx]->{'path_mysock'},
                  port     => $master->[$idx]->{'path_myport'},
                 });
      $master->[$idx]->{'pid'}= 0; # Assume we are done with it
    }
  }

  if ( ! $master->[0]->{'ndbcluster'} )
  {
    ndbcluster_stop();
    $master->[0]->{'ndbcluster'}= 1;
  }

  mtr_stop_mysqld_servers(\@args);
}

sub stop_slaves () {
  my $force= shift;

  my @args;

  for ( my $idx; $idx < 3; $idx++ )
  {
    if ( $slave->[$idx]->{'pid'} )
    {
      push(@args,{
                  pid      => $slave->[$idx]->{'pid'},
                  pidfile  => $slave->[$idx]->{'path_mypid'},
                  sockfile => $slave->[$idx]->{'path_mysock'},
                  port     => $slave->[$idx]->{'path_myport'},
                 });
      $slave->[$idx]->{'pid'}= 0; # Assume we are done with it
    }
  }

  mtr_stop_mysqld_servers(\@args);
}


sub run_mysqltest ($$) {
  my $tinfo=       shift;
  my $master_opts= shift;

  my $cmdline_mysqldump= "$exe_mysqldump --no-defaults -uroot " .
                         "--socket=$master->[0]->{'path_mysock'} --password=";
  if ( $opt_debug )
  {
    $cmdline_mysqldump .=
      " --debug=d:t:A,$glob_mysql_test_dir/var/log/mysqldump.trace";
  }

  my $cmdline_mysqlbinlog=
    "$exe_mysqlbinlog --no-defaults --local-load=$opt_tmpdir";

  if ( $opt_debug )
  {
    $cmdline_mysqlbinlog .=
      " --debug=d:t:A,$glob_mysql_test_dir/var/log/mysqlbinlog.trace";
  }

  my $cmdline_mysql=
    "$exe_mysql --host=localhost  --user=root --password= " .
    "--port=$master->[0]->{'path_myport'} " .
    "--socket=$master->[0]->{'path_mysock'}";

  my $cmdline_mysql_client_test=
    "$exe_mysql_client_test --no-defaults --testcase --user=root --silent " .
    "--port=$master->[0]->{'path_myport'} " .
    "--socket=$master->[0]->{'path_mysock'}";

  my $cmdline_mysql_fix_system_tables=
    "$exe_mysql_fix_system_tables --no-defaults --host=localhost --user=root --password= " .
    "--basedir=$glob_basedir --bindir=$path_client_bindir --verbose " .
    "--port=$master->[0]->{'path_myport'} " .
    "--socket=$master->[0]->{'path_mysock'}";



  # FIXME really needing a PATH???
  # $ENV{'PATH'}= "/bin:/usr/bin:/usr/local/bin:/usr/bsd:/usr/X11R6/bin:/usr/openwin/bin:/usr/bin/X11:$ENV{'PATH'}";

  $ENV{'MYSQL'}=                    $cmdline_mysql;
  $ENV{'MYSQL_DUMP'}=               $cmdline_mysqldump;
  $ENV{'MYSQL_BINLOG'}=             $cmdline_mysqlbinlog;
  $ENV{'MYSQL_FIX_SYSTEM_TABLES'}=  $cmdline_mysql_fix_system_tables;
  $ENV{'MYSQL_CLIENT_TEST'}=        $cmdline_mysql_client_test;
  $ENV{'CHARSETSDIR'}=              $path_charsetsdir;

  $ENV{'NDB_STATUS_OK'}=            $flag_ndb_status_ok;
  $ENV{'NDB_MGM'}=                  $exe_ndb_mgm;
  $ENV{'NDB_BACKUP_DIR'}=           $path_ndb_backup_dir;
  $ENV{'NDB_TOOLS_DIR'}=            $path_ndb_tools_dir;
  $ENV{'NDB_TOOLS_OUTPUT'}=         $file_ndb_testrun_log;
  $ENV{'NDB_CONNECTSTRING'}=        $opt_ndbconnectstring;

  my $exe= $exe_mysqltest;
  my $args;

  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--socket=%s", $master->[0]->{'path_mysock'});
  mtr_add_arg($args, "--database=test");
  mtr_add_arg($args, "--user=%s", $opt_user);
  mtr_add_arg($args, "--password=");
  mtr_add_arg($args, "--silent");
  mtr_add_arg($args, "-v");
  mtr_add_arg($args, "--skip-safemalloc");
  mtr_add_arg($args, "--tmpdir=%s", $opt_tmpdir);
  mtr_add_arg($args, "--port=%d", $master->[0]->{'path_myport'});

  if ( $opt_ps_protocol )
  {
    mtr_add_arg($args, "--ps-protocol");
  }

  if ( $opt_strace_client )
  {
    $exe=  "strace";            # FIXME there are ktrace, ....
    mtr_add_arg($args, "-o");
    mtr_add_arg($args, "%s/var/log/mysqltest.strace", $glob_mysql_test_dir);
    mtr_add_arg($args, "$exe_mysqltest");
  }

  if ( $opt_timer )
  {
    mtr_add_arg($args, "--timer-file=%s/var/log/timer", $glob_mysql_test_dir);
  }

  if ( $opt_big_test )
  {
    mtr_add_arg($args, "--big-test");
  }

  if ( $opt_record )
  {
    mtr_add_arg($args, "--record");
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
    mtr_add_arg($args, "--debug=d:t:A,%s/var/log/mysqltest.trace",
                $glob_mysql_test_dir);
  }

  if ( $opt_with_openssl )
  {
    mtr_add_arg($args, "--ssl-ca=%s/SSL/cacert.pem", $glob_basedir);
    mtr_add_arg($args, "--ssl-cert=%s/SSL/client-cert.pem", $glob_basedir);
    mtr_add_arg($args, "--ssl-key=%s/SSL/client-key.pem", $glob_basedir);
  }

  mtr_add_arg($args, "-R");
  mtr_add_arg($args, $tinfo->{'result_file'});

  # ----------------------------------------------------------------------
  # If embedded server, we create server args to give mysqltest to pass on
  # ----------------------------------------------------------------------

  if ( $glob_use_embedded_server )
  {
    mysqld_arguments($args,'master',0,$tinfo->{'master_opt'},[]);
  }

  return mtr_run_test($exe,$args,$tinfo->{'path'},"",$path_timefile,"");
}

##############################################################################
#
#  Usage
#
##############################################################################

sub usage ($)
{
  print STDERR <<HERE;

mysql-test-run [ OPTIONS ] [ TESTCASE ]

FIXME when is TESTCASE arg used or not?!

Options to control what engine/variation to run

  embedded-server       Use the embedded server, i.e. no mysqld daemons
  ps-protocol           Use the binary protocol between client and server
  bench                 Run the benchmark suite FIXME
  small-bench           FIXME
  no-manager            Use the istanse manager (currently disabled)

Options to control what test suites or cases to run

  force                 Continue to run the suite after failure
  with-ndbcluster       Use cluster, and enable test cases that requres it
  do-test=PREFIX        Run test cases which name are prefixed with PREFIX
  start-from=PREFIX     Run test cases starting from test prefixed with PREFIX
  suite=NAME            Run the test suite named NAME. The default is "main"
  skip-rpl              Skip the replication test cases.
  skip-test=PREFIX      Skip test cases which name are prefixed with PREFIX

Options that specify ports

  master_port=PORT      Specify the port number used by the first master
  slave_port=PORT       Specify the port number used by the first slave
  ndbcluster_port=i     Specify the port number used by cluster FIXME

Options for test case authoring

  record TESTNAME       (Re)genereate the result file for TESTNAME

Options that pass on options

  mysqld=ARGS           Specify additional arguments to "mysqld"

Options to run test on running server

  extern                Use running server for tests FIXME DANGEROUS
  ndbconnectstring=STR  Use running cluster, and connect using STR      
  user=USER             User for connect to server

Options for debugging the product

  gdb                   FIXME
  manual-gdb            FIXME
  client-gdb            FIXME
  ddd                   FIXME
  strace-client         FIXME
  master-binary=PATH    Specify the master "mysqld" to use
  slave-binary=PATH     Specify the slave "mysqld" to use

Options for coverage, profiling etc

  gcov                  FIXME
  gprof                 FIXME
  valgrind              FIXME
  valgrind-all          FIXME
  valgrind-options=ARGS Extra options to give valgrind

Misc options

  verbose               Verbose output from this script
  script-debug          Debug this script itself
  compress              Use the compressed protocol between client and server
  timer                 Show test case execution time
  start-and-exit        Only initiate and start the "mysqld" servers
  start-dirty           Only start the "mysqld" servers without initiation
  fast                  Don't try to cleanup from earlier runs
  reorder               Reorder tests to get less server restarts
  help                  Get this help text
  unified-diff | udiff  When presenting differences, use unified diff

Options not yet described, or that I want to look into more

  big-test              
  debug                 
  local                 
  local-master          
  netware               
  old-master            
  sleep=SECONDS         
  socket=PATH           
  tmpdir=DIR            
  user-test=s           
  wait-timeout=SECONDS  
  warnings              
  log-warnings          
  with-openssl          

HERE
  exit(1);
}
