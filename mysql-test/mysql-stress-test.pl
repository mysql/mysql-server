#!/usr/bin/perl

# Copyright (c) 2005, 2019, Oracle and/or its affiliates. All rights reserved.
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

# ======================================================================
#                     MySQL server stress test system 
# ======================================================================
#
##########################################################################
#
#                       SCENARIOS AND REQUIREMENTS
#
#   The system should perform stress testing of MySQL server with 
# following requirements and basic scenarios:
# 
# Basic requirements:
# 
# Design of stress script should allow one:
# 
#   - to use for stress testing mysqltest binary as test engine
#   - to use for stress testing both regular test suite and any
#     additional test suites (e.g. mysql-test-extra-5.0)
#   - to specify files with lists of tests both for initialization of
#     stress db and for further testing itself
#   - to define number of threads that will be concurrently used in testing
#   - to define limitations for test run. e.g. number of tests or loops
#     for execution or duration of testing, delay between test executions, etc.
#   - to get readable log file which can be used for identification of
#     errors arose during testing
# 
# Basic scenarios:
# 
#     * It should be possible to run stress script in standalone mode
#       which will allow to create various scenarios of stress workloads:
# 
#       simple ones:
#
#         box #1:
#           - one instance of script with list of tests #1
# 
#       and more advanced ones:
# 
#         box #1:
#           - one instance of script with list of tests #1
#           - another instance of script with list of tests #2
#         box #2:
#           - one instance of script with list of tests #3
#           - another instance of script with list of tests #4
#             that will recreate whole database to back it to clean
#             state
# 
#       One kind of such complex scenarios maybe continued testing
#       when we want to run stress tests from many boxes with various
#       lists of tests that will last very long time. And in such case
#       we need some wrapper for MySQL server that will restart it in
#       case of crashes.
# 
#     * It should be possible to run stress script in ad-hoc mode from
#       shell or perl versions of mysql-test-run. This allows developers
#       to reproduce and debug errors that was found in continued stress 
#       testing
#
# 2009-01-28 OBN Additions and modifications per WL#4685
#                
########################################################################

use Config;

if (!defined($Config{useithreads}))
{
  die <<EOF;
It is unable to run threaded version of stress test on this system 
due to disabled ithreads. Please check that installed perl binary 
was built with support of ithreads. 
EOF
}

use threads;
use threads::shared;

use IO::Socket;
use Sys::Hostname;
use File::Copy;
use File::Spec;
use File::Find;
use File::Basename;
use File::Path;
use Cwd;

use Data::Dumper;
use Getopt::Long;

my $stress_suite_version="1.0";

$|=1;

$opt_server_host="";
$opt_server_logs_dir="";
$opt_help="";
$opt_server_port="";
$opt_server_socket="";
$opt_server_user="";
$opt_server_password="";
$opt_server_database="";
$opt_cleanup="";
$opt_verbose="";
$opt_log_error_details="";


$opt_suite="main";
$opt_stress_suite_basedir="";
$opt_stress_basedir="";
$opt_stress_datadir="";
$opt_test_suffix="";

$opt_stress_mode="random";

$opt_loop_count=0;
$opt_test_count=0;
$opt_test_duration=0;
# OBN: Changing abort-on-error default to -1 (for WL-4626/4685): -1 means no abort
$opt_abort_on_error=-1;
$opt_sleep_time = 0;
$opt_threads=1;
$pid_file="mysql_stress_test.pid";
$opt_mysqltest= ($^O =~ /mswin32/i) ? "mysqltest.exe" : "mysqltest";
$opt_check_tests_file="";
# OBM adding a setting for 'max-connect-retries=20' the default of 500 is to high  
@mysqltest_args=("--silent", "-v", "--max-connect-retries=20");

# Client ip address
$client_ip=inet_ntoa((gethostbyname(hostname()))[4]);
$client_ip=~ s/\.//g;

%tests_files=(client => {mtime => 0, data => []},
              initdb => {mtime => 0, data => []});

# Error codes and sub-strings with corresponding severity 
#
# S1 - Critical errors - cause immediately abort of testing. These errors
#                        could be caused by server crash or impossibility
#                        of test execution. 
#
# S2 - Serious errors - these errors are bugs for sure as it knowns that 
#                       they shouldn't appear during stress testing  
#
# S3 - Unknown errors - Errors were returned but we don't know what they are 
#                       so script can't determine if they are OK or not
#
# S4 - Non-seriuos errros - these errors could be caused by fact that 
#                           we execute simultaneously statements that
#                           affect tests executed by other threads
                            
%error_strings = ( 'Failed in mysql_real_connect()' => S1,
                   'Can\'t connect' => S1,       
                   'not found (Errcode: 2)' => S1,
                   'does not exist' => S1,
                   'Could not open connection \'default\' after \d+ attempts' => S1,
                   'wrong errno ' => S3,
                   'Result length mismatch' => S4,
                   'Result content mismatch' => S4);
  
%error_codes = ( 1012 => S2, 1015 => S2, 1021 => S2,
                 1027 => S2, 1037 => S2, 1038 => S2,
                 1039 => S2, 1040 => S2, 1046 => S2, 
                 1053 => S2, 1180 => S2, 1181 => S2, 
                 1203 => S2, 1205 => S4, 1206 => S2, 
                 1207 => S2, 1213 => S4, 1223 => S2, 
                 2002 => S1, 2003 => S1, 2006 => S1,
                 2013 => S1 
                 );

share(%test_counters);
%test_counters=( loop_count => 0, test_count=>0);

share($exiting);
$exiting=0;

# OBN Code and 'set_exit_code' function added by ES to set an exit code based on the error category returned 
#     in combination with the --abort-on-error value see WL#4685)
use constant ABORT_MAKEWEIGHT => 20;  
share($gExitCode);
$gExitCode = 0;   # global exit code
sub set_exit_code {
	my $severity = shift;
	my $code = 0;
	if ( $severity =~ /^S(\d+)/ ) {
		$severity = $1;
		$code = 11 - $severity; # S1=10, S2=9, ... -- as per WL
	}
	else {
	# we know how we call the sub: severity should be S<num>; so, we should never be here...
		print STDERR "Unknown severity format: $severity; setting to S1\n";
		$severity = 1;
	}
	$abort = 0;
	if ( $severity <= $opt_abort_on_error ) {
		# the test finished with a failure severe enough to abort. We are adding the 'abort flag' to the exit code
		$code += ABORT_MAKEWEIGHT;
		# but are not exiting just yet -- we need to update global exit code first
		$abort = 1;
	}
	lock $gExitCode; # we can use lock here because the script uses threads anyway
	$gExitCode = $code if $code > $gExitCode;
	kill INT, $$ if $abort; # this is just a way to call sig_INT_handler: it will set exiting flag, which should do the rest
}

share($test_counters_lock);
$test_counters_lock=0;
share($log_file_lock);
$log_file_lock=0;

$SIG{INT}= \&sig_INT_handler;
$SIG{TERM}= \&sig_TERM_handler;


GetOptions("server-host=s", "server-logs-dir=s", "server-port=s",
           "server-socket=s", "server-user=s", "server-password=s",
           "server-database=s",
           "stress-suite-basedir=s", "suite=s", "stress-init-file:s", 
           "stress-tests-file:s", "stress-basedir=s", "stress-mode=s",
           "stress-datadir=s",
           "threads=s", "sleep-time=s", "loop-count=i", "test-count=i",
           "test-duration=i", "test-suffix=s", "check-tests-file", 
           "verbose", "log-error-details", "cleanup", "mysqltest=s", 
           # OBN: (changing 'abort-on-error' to numberic for WL-4626/4685) 
           "abort-on-error=i" => \$opt_abort_on_error, "help") || usage(1);

usage(0) if ($opt_help);

#$opt_abort_on_error=1;

$test_dirname=get_timestamp();
$test_dirname.="-$opt_test_suffix" if ($opt_test_suffix ne '');

print <<EOF;
#############################################################
                  CONFIGURATION STAGE
#############################################################
EOF

if ($opt_stress_basedir eq '' || $opt_stress_suite_basedir eq '' ||
    $opt_server_logs_dir eq '')
{
  die <<EOF;

Options --stress-basedir, --stress-suite-basedir and --server-logs-dir are 
required. Please use these options to specify proper basedir for 
client, test suite and location of server logs.

stress-basedir: '$opt_stress_basedir'
stress-suite-basedir: '$opt_stress_suite_basedir'
server-logs-dir: '$opt_server_logs_dir'

EOF
}

#Workaround for case when we got relative but not absolute path 
$opt_stress_basedir=File::Spec->rel2abs($opt_stress_basedir);
$opt_stress_suite_basedir=File::Spec->rel2abs($opt_stress_suite_basedir);
$opt_server_logs_dir=File::Spec->rel2abs($opt_server_logs_dir);

if ($opt_stress_datadir ne '')
{
  $opt_stress_datadir=File::Spec->rel2abs($opt_stress_datadir);
}

if (! -d "$opt_stress_basedir")
{
  die <<EOF;
  
Directory '$opt_stress_basedir' does not exist.
Use --stress-basedir option to specify proper basedir for client

EOF
}

if (!-d $opt_stress_suite_basedir)
{
  die <<EOF;

Directory '$opt_stress_suite_basedir' does not exist.
Use --stress-suite-basedir option to specify proper basedir for test suite

EOF
}

$test_dataset_dir=$opt_stress_suite_basedir;
if ($opt_stress_datadir ne '')
{
  if (-d $opt_stress_datadir)
  {
    $test_dataset_dir=$opt_stress_datadir;

  }
  else
  {
    die <<EOF;
Directory '$opt_stress_datadir' not exists. Please specify proper one 
with --stress-datadir option.
EOF
  }  
}

if ($^O =~ /mswin32/i)
{
  $test_dataset_dir=~ s/\\/\\\\/g;
}
else
{
  $test_dataset_dir.="/";
}



if (!-d $opt_server_logs_dir)
{
  die <<EOF;

Directory server-logs-dir '$opt_server_logs_dir' does not exist.
Use --server-logs-dir option to specify proper directory for storing 
logs 

EOF
}
else
{
  #Create sub-directory for test session logs
  mkpath(File::Spec->catdir($opt_server_logs_dir, $test_dirname), 0, 0755);
  #Define filename of global session log file
  $stress_log_file=File::Spec->catfile($opt_server_logs_dir, $test_dirname,
                                       "mysql-stress-test.log");
}

if ($opt_suite ne '' && $opt_suite ne 'main' && $opt_suite ne 'default')
{
  $test_suite_dir=File::Spec->catdir($opt_stress_suite_basedir, "suite", $opt_suite);
}
else
{
  $test_suite_dir= $opt_stress_suite_basedir;
}

if (!-d $test_suite_dir)
{
  die <<EOF

Directory '$test_suite_dir' does not exist.
Use --suite options to specify proper dir for test suite

EOF
}

$test_suite_t_path=File::Spec->catdir($test_suite_dir,'t');
$test_suite_r_path=File::Spec->catdir($test_suite_dir,'r');

foreach my $suite_dir ($test_suite_t_path, $test_suite_r_path)
{
  if (!-d $suite_dir)
  {
    die <<EOF;

Directory '$suite_dir' does not exist.
Please ensure that you specified proper source location for 
test/result files with --stress-suite-basedir option and name 
of test suite with --suite option

EOF
  }
}

$test_t_path=File::Spec->catdir($opt_stress_basedir,'t');
$test_r_path=File::Spec->catdir($opt_stress_basedir,'r');

foreach $test_dir ($test_t_path, $test_r_path)
{
  if (-d $test_dir)
  {
    if ($opt_cleanup)
    {
      #Delete existing 't', 'r', 'r/*' subfolders in $stress_basedir
      rmtree("$test_dir", 0, 0);
      print "Cleanup $test_dir\n";
    }
    else
    {
      die <<EOF;
Directory '$test_dir' already exist. 
Please ensure that you specified proper location of working dir
for current test run with --stress-basedir option or in case of staled
directories use --cleanup option to remove ones
EOF
    }
  }
  #Create empty 't', 'r' subfolders that will be filled later
  mkpath("$test_dir", 0, 0777);
}

if (!defined($opt_stress_tests_file) && !defined($opt_stress_init_file))
{
  die <<EOF;
You should run stress script either with --stress-tests-file or with 
--stress-init-file otions. See help for details.
EOF
}

if (defined($opt_stress_tests_file))
{ 
  if ($opt_stress_tests_file eq '')
  {
    #Default location of file with set of tests for current test run
    $tests_files{client}->{filename}= File::Spec->catfile($opt_stress_suite_basedir,
                                      "testslist_client.txt");
  }
  else
  {
    $tests_files{client}->{filename}= $opt_stress_tests_file;
  }

  if (!-f $tests_files{client}->{filename})
  {
    die <<EOF;

File '$tests_files{client}->{filename}' with list of tests not exists. 
Please ensure that this file exists, readable or specify another one with 
--stress-tests-file option.

EOF
  }
}

if (defined($opt_stress_init_file))
{
  if ($opt_stress_init_file eq '')
  {
    #Default location of file with set of tests for current test run
    $tests_files{initdb}->{filename}= File::Spec->catfile($opt_stress_suite_basedir,
                                      "testslist_initdb.txt");
  }
  else
  {
    $tests_files{initdb}->{filename}= $opt_stress_init_file;
  }

  if (!-f $tests_files{initdb}->{filename})
  {
    die <<EOF;

File '$tests_files{initdb}->{filename}' with list of tests for initialization of database
for stress test not exists. 
Please ensure that this file exists, readable or specify another one with 
--stress-init-file option.

EOF
  }
}

if ($opt_stress_mode !~ /^(random|seq)$/)
{
  die <<EOF
Was specified wrong --stress-mode. Correct values 'random' and 'seq'.
EOF
}

if (open(TEST, "$opt_mysqltest -V |"))
{
  $mysqltest_version=join("",<TEST>);
  close(TEST);
  print "FOUND MYSQLTEST BINARY: ", $mysqltest_version,"\n";
}
else
{
  die <<EOF;
ERROR: mysqltest binary $opt_mysqltest not found $!.
You must either specify file location explicitly using --mysqltest
option, or make sure path to mysqltest binary is listed 
in your PATH environment variable.
EOF
}

#        
#Adding mysql server specific command line options for mysqltest binary
#
$opt_server_host= $opt_server_host ? $opt_server_host : "localhost";
$opt_server_port= $opt_server_port ? $opt_server_port : "3306";
$opt_server_user= $opt_server_user ? $opt_server_user : "root";
$opt_server_socket= $opt_server_socket ? $opt_server_socket : "/tmp/mysql.sock";
$opt_server_database= $opt_server_database ? $opt_server_database : "test";

unshift @mysqltest_args, "--host=$opt_server_host";
unshift @mysqltest_args, "--port=$opt_server_port";
unshift @mysqltest_args, "--user=$opt_server_user";
unshift @mysqltest_args, "--password=$opt_server_password";
unshift @mysqltest_args, "--socket=$opt_server_socket";
unshift @mysqltest_args, "--database=$opt_server_database";

#Export variables that could be used in tests
$ENV{MYSQL_TEST_DIR}=$test_dataset_dir;
$ENV{MASTER_MYPORT}=$opt_server_port;
$ENV{MASTER_MYSOCK}=$opt_server_socket;

print <<EOF;
TEST-SUITE-BASEDIR: $opt_stress_suite_basedir
SUITE:              $opt_suite
TEST-BASE-DIR:      $opt_stress_basedir
TEST-DATADIR:       $test_dataset_dir
SERVER-LOGS-DIR:    $opt_server_logs_dir

THREADS:            $opt_threads
TEST-MODE:          $opt_stress_mode

EOF

#-------------------------------------------------------------------------------
#At this stage we've already checked all needed pathes/files 
#and ready to start the test
#-------------------------------------------------------------------------------

if (defined($opt_stress_tests_file) || defined($opt_stress_init_file))
{
  print <<EOF;
#############################################################
                  PREPARATION STAGE
#############################################################
EOF

  #Copy Test files from network share to 't' folder
  print "\nCopying Test files from $test_suite_t_path to $test_t_path folder...";
  find({wanted=>\&copy_test_files, bydepth=>1}, "$test_suite_t_path");
  print "Done\n";

  #$test_r_path/r0 dir reserved for initdb
  $count_start= defined($opt_stress_init_file) ? 0 : 1;

  our $r_folder='';
  print "\nCreating 'r' folder and copying Protocol files to each 'r#' sub-folder...";
  for($count=$count_start; $count <= $opt_threads; $count++)
  {
    $r_folder = File::Spec->catdir($test_r_path, "r".$count);
    mkpath("$r_folder", 0, 0777); 
     
    find(\&copy_result_files,"$test_suite_r_path");
  }  
  print "Done\n\n";
}

if (defined($opt_stress_init_file))
{
  print <<EOF;
#############################################################
                  INITIALIZATION STAGE
#############################################################
EOF

  #Set limits for stress db initialization 
  %limits=(loop_count => 1, test_count => undef);

  #Read list of tests from $opt_stress_init_file
  read_tests_names($tests_files{initdb});
  test_loop($client_ip, 0, 'seq', $tests_files{initdb});  
  #print Dumper($tests_files{initdb}),"\n";
  print <<EOF;

Done initialization of stress database by tests from 
$tests_files{initdb}->{filename} file.

EOF
}

if (defined($opt_stress_tests_file))
{
  print <<EOF;
#############################################################
                  STRESS TEST RUNNING STAGE
#############################################################
EOF

  $exiting=0;
  #Read list of tests from $opt_stress_tests_file 
  read_tests_names($tests_files{client});

  #Reset current counter and set limits
  %test_counters=( loop_count => 0, test_count=>0);
  %limits=(loop_count => $opt_loop_count, test_count => $opt_test_count);

  if (($opt_loop_count && $opt_threads > $opt_loop_count) || 
      ($opt_test_count && $opt_threads > $opt_test_count))
  {
    warn <<EOF;

WARNING: Possible inaccuracies in number of executed loops or 
         tests because number of threads bigger than number of 
         loops or tests:
         
         Threads will be started: $opt_threads
         Loops will be executed:  $opt_loop_count
         Tests will be executed:  $opt_test_count    

EOF
  }

  #Create threads (number depending on the variable )
  for ($id=1; $id<=$opt_threads && !$exiting; $id++)
  {
    $thrd[$id] = threads->create("test_loop", $client_ip, $id,
                                 $opt_stress_mode, $tests_files{client});

    print "main: Thread ID $id TID ",$thrd[$id]->tid," started\n";
    select(undef, undef, undef, 0.5);
  }

  if ($opt_test_duration)
  {
  # OBN - At this point we need to wait for the duration of the test, hoever
  #       we need to be able to quit if an 'abort-on-error' condition has happend 
  #       with one of the children (WL#4685). Using solution by ES and replacing 
  #       the 'sleep' command with a loop checking the abort condition every second
  
	foreach ( 1..$opt_test_duration ) {       
		last if $exiting;                     
		sleep 1;                              
	}
    kill INT, $$;                             #Interrupt child threads
  }

  #Let other threads to process INT signal
  sleep(1);

  for ($id=1; $id<=$opt_threads;$id++)
  {
    if (defined($thrd[$id]))
    {
      $thrd[$id]->join();
    }
  }
  print "EXIT\n";
}

exit $gExitCode; # ES WL#4685: script should return a meaningful exit code

sub test_init
{
  my ($env)=@_;
  
  $env->{session_id}=$env->{ip}."_".$env->{thread_id};
  $env->{r_folder}='r'.$env->{thread_id}; 
  $env->{screen_logs}=File::Spec->catdir($opt_server_logs_dir, $test_dirname, 
                                         "screen_logs", $env->{session_id});
  $env->{reject_logs}=File::Spec->catdir($opt_server_logs_dir, $test_dirname,
                                         "reject_logs", $env->{session_id});
  
  mkpath($env->{screen_logs}, 0, 0755) unless (-d $env->{screen_logs});
  mkpath($env->{reject_logs}, 0, 0755) unless (-d $env->{reject_logs});

  $env->{session_log}= File::Spec->catfile($env->{screen_logs}, $env->{session_id}.".log");     
}

sub test_execute
{
  my $env= shift;
  my $test_name= shift;

  my $g_start= "";
  my $g_end= "";
  my $mysqltest_cmd= "";
  my @mysqltest_test_args=();
  my @stderr=();

  #Get time stamp
  $g_start = get_timestamp();
  $env->{errors}={};
  @{$env->{test_status}}=();

  my $test_file= $test_name.".test";
  my $result_file= $test_name.".result";
  my $reject_file = $test_name.'.reject';
  my $output_file = $env->{session_id}.'_'.$test_name.'_'.$g_start."_".$env->{test_count}.'.txt';

  my $test_filename = File::Spec->catfile($test_t_path, $test_file);
  my $result_filename = File::Spec->catdir($test_r_path, $env->{r_folder}, $result_file);
  my $reject_filename = File::Spec->catdir($test_r_path, $env->{r_folder}, $reject_file);
  my $output_filename = File::Spec->catfile($env->{screen_logs}, $output_file);     


  push @mysqltest_test_args, "--basedir=$opt_stress_suite_basedir/",
                             "--tmpdir=$opt_stress_basedir",
                             "-x $test_filename",
                             "-R $result_filename",
                             "2>$output_filename";
                        
  $cmd= "$opt_mysqltest --no-defaults ".join(" ", @mysqltest_args)." ".
                                        join(" ", @mysqltest_test_args);

  system($cmd);

  $exit_value  = $? >> 8;
  $signal_num  = $? & 127;
  $dumped_core = $? & 128;

  my $tid= threads->self->tid;

  if (-s $output_filename > 0)
  { 
    #Read stderr for further analysis
    open (STDERR_LOG, $output_filename) or 
                             warn "Can't open file $output_filename";
    @stderr=<STDERR_LOG>;
    close(STDERR_LOG);
 
    if ($opt_verbose)
    {
      $session_debug_file="$opt_stress_basedir/error$tid.txt";
      
      stress_log($session_debug_file, 
                "Something wrong happened during execution of this command line:");
      stress_log($session_debug_file, "MYSQLTEST CMD - $cmd");    
      stress_log($session_debug_file, "STDERR:".join("",@stderr));      

      stress_log($session_debug_file, "EXIT STATUS:\n1. EXIT: $exit_value \n".
                                      "2. SIGNAL: $signal_num\n".
                                      "3. CORE: $dumped_core\n");
    }
  }

  #If something wrong trying to analyse stderr 
  if ($exit_value || $signal_num)
  {
    if (@stderr)
    {
      foreach my $line (@stderr)
      {
        #FIXME: we should handle case when for one sub-string/code 
        #       we have several different error messages        
        #       Now for both codes/substrings we assume that
        #       first found message will represent error

        #Check line for error codes
        if (($err_msg, $err_code)= $line=~/failed: ((\d+):.+?$)/)
        {
          if (!exists($error_codes{$err_code}))
          {
            # OBN Changing severity level to S4 from S3 as S3 now reserved
            #     for the case where the error is unknown (for WL#4626/4685
            $severity="S4";
            $err_code=0;
          }
          else
          {
            $severity=$error_codes{$err_code};
          }

          if (!exists($env->{errors}->{$severity}->{$err_code}))
          {
            $env->{errors}->{$severity}->{$err_code}=[0, $err_msg];
          }
          $env->{errors}->{$severity}->{$err_code}->[0]++;
          $env->{errors}->{$severity}->{total}++;          
        }

        #Check line for error patterns
        foreach $err_string (keys %error_strings)
        {
          $pattern= quotemeta $err_string;
          if ($line =~ /$pattern/i)
          {
            my $severity= $error_strings{$err_string};
            if (!exists($env->{errors}->{$severity}->{$err_string}))
            {
              $env->{errors}->{$severity}->{$err_string}=[0, $line];
            }
            $env->{errors}->{$severity}->{$err_string}->[0]++;
            $env->{errors}->{$severity}->{total}++;          
          }
        }
      }
    }
    else
    {
      $env->{errors}->{S3}->{'Unknown error'}=
                              [1,"Unknown error. Nothing was output to STDERR"];
      $env->{errors}->{S3}->{total}=1;
    }
  }

  #
  #FIXME: Here we can perform further analysis of recognized 
  #       error codes 
  #

  foreach my $severity (sort {$a cmp $b} keys %{$env->{errors}})
  {
    my $total=$env->{errors}->{$severity}->{total};
    if ($total)
    {
      push @{$env->{test_status}}, "Severity $severity: $total";
      $env->{errors}->{total}=+$total;
      set_exit_code($severity);  
    }
  }

  #FIXME: Should we take into account $exit_value here?
  #       Now we assume that all stringified errors(i.e. errors without 
  #       error codes) which are not exist in %error_string structure 
  #       are OK
  if (!$env->{errors}->{total})
  {
    push @{$env->{test_status}},"No Errors. Test Passed OK";
  }

  log_session_errors($env, $test_file);

  #OBN Removing the case of S1 and abort-on-error as that is now set 
  #     inside the set_exit_code function (for WL#4626/4685)
  #if (!$exiting && ($signal_num == 2 || $signal_num == 15 || 
  #       ($opt_abort_on_error && $env->{errors}->{S1} > 0)))
  if (!$exiting && ($signal_num == 2 || $signal_num == 15))
  {
    #mysqltest was interrupted with INT or TERM signals 
    #so we assume that we should cancel testing and exit
    $exiting=1;
    # OBN - Adjusted text to exclude case of S1 and abort-on-error that 
    #       was mentioned (for WL#4626/4685)
    print STDERR<<EOF;
WARNING:
   mysqltest was interrupted with INT or TERM signals  so we assume that 
   we should cancel testing and exit. Please check log file for this thread 
   in $stress_log_file or 
   inspect below output of the last test case executed with mysqltest to 
   find out cause of error.
   
   Output of mysqltest:
   @stderr
   
EOF
  }

  if (-e $reject_filename)
  {  
    move_to_logs($env->{reject_logs}, $reject_filename, $reject_file);
  }    
  
  if (-e $output_filename)
  {  
    move_to_logs($env->{screen_logs}, $output_filename, $output_file);
  }    

}

sub test_loop
{     
  my %client_env=();
  my $test_name="";

  # KEY for session identification: IP-THREAD_ID
  $client_env{ip} = shift;
  $client_env{thread_id} = shift;

  $client_env{mode} = shift;
  $client_env{tests_file}=shift; 

  $client_env{test_seq_idx}=0;

  #Initialize session variables
  test_init(\%client_env);

LOOP:

  while(!$exiting)
  {
    if ($opt_check_tests_file)
    {
      #Check if tests_file was modified and reread it in this case
      read_tests_names($client_env{tests_file}, 0);
    }

    {
      lock($test_counters_lock);

      if (($limits{loop_count} && $limits{loop_count} <= $test_counters{loop_count}*1) ||
          ($limits{test_count} && $limits{test_count} <= $test_counters{test_count}*1) )
      {
        $exiting=1;
        next LOOP;
      }
    }

    #Get random file name 
    if (($test_name = get_test(\%client_env)) ne '')
    {
      {
        lock($test_counters_lock);

        #Save current counters values 
        $client_env{loop_count}=$test_counters{loop_count};
        $client_env{test_count}=$test_counters{test_count};
      }
      #Run test and analyze results
      test_execute(\%client_env, $test_name);

      print "test_loop[".$limits{loop_count}.":".
             $limits{test_count}." ".
             $client_env{loop_count}.":".
             $client_env{test_count}."]:".
             " TID ".$client_env{thread_id}.
             " test: '$test_name' ".
             " Errors: ".join(" ",@{$client_env{test_status}}).
                ( $exiting ? " (thread aborting)" : "" )."\n";
    }
  
    # OBN - At this point we need to wait until the 'wait' time between test
    #       executions passes (in case it is specifed) passes, hoever we need
    #       to be able to quit and break out of the test if an 'abort-on-error' 
    #       condition has happend with one of the other children (WL#4685). 
    #       Using solution by ES and replacing the 'sleep' command with a loop 
    #       checking the abort condition every second
  
	if ( $opt_sleep_time ) {                
		foreach ( 1..$opt_sleep_time ) {     
			last if $exiting;               
			sleep 1;                        
		}                                   
	}                                       
  }
}

sub move_to_logs ($$$)
{
  my $path_to_logs = shift;
  my $src_file = shift;
  my $random_filename = shift;

  my $dst_file = File::Spec->catfile($path_to_logs, $random_filename);
  
  move ($src_file, $dst_file) or warn<<EOF;
ERROR: move_to_logs: File $src_file cannot be moved to $dst_file: $!
EOF
}

sub copy_test_files ()
{
  if (/\.test$/)
  { 
    $src_file = $File::Find::name;
    #print "## $File::Find::topdir - $File::Find::dir - $src_file\n";

    if ($File::Find::topdir eq $File::Find::dir && $src_file !~ /SCCS/)
    {
      $test_filename = basename($src_file);
      $dst_file = File::Spec->catfile($test_t_path, $test_filename);

      copy($src_file, $dst_file) or die "ERROR: copy_test_files: File cannot be copied. $!";
    }
  }
}

sub copy_result_files ()
{
  if (/\.result$/)
  { 
    $src_file = $File::Find::name;

    if ($File::Find::topdir eq $File::Find::dir && $src_file !~ /SCCS/)
    {
      $result_filename = basename($src_file) ;
      $dst_file = File::Spec->catfile($r_folder, $result_filename);

      copy($src_file, $dst_file) or die "ERROR: copy_result_files: File cannot be copied. $!";
    }
  }
}

sub get_timestamp
{
  my ($sec,$min,$hour,$mday,$mon,$year,$wday,$ydat,$isdst) = localtime();

  return sprintf("%04d%02d%02d%02d%02d%02d", $year+1900, $mon+1, $mday, $hour, $min, $sec);
}

sub read_tests_names
{
  my $tests_file = shift;
  my $force_load = shift;

  if ($force_load || ( (stat($tests_file->{filename}))[9] != $tests_file->{mtime}) )
  {
    open (TEST, $tests_file->{filename}) || die ("Could not open file <".
                                                  $tests_file->{filename}."> $!");
    @{$tests_file->{data}}= grep {!/^[#\r\n]|^$/} map { s/[\r\n]//g; $_ } <TEST>;

    close (TEST); 
    $tests_file->{mtime}=(stat(_))[9];
  }
}

sub get_random_test
{
  my $envt=shift;
  my $tests= $envt->{tests_file}->{data};

  my $random = int(rand(@{$tests}));
  my $test = $tests->[$random];

  return $test;
}

sub get_next_test
{
  my $envt=shift;
  my $test;

  if (@{$envt->{tests_file}->{data}})
  {
    $test=${$envt->{tests_file}->{data}}[$envt->{test_seq_idx}];
    $envt->{test_seq_idx}++;
  }
  
  #If we reach bound of array, reset seq index and increment loop counter
  if ($envt->{test_seq_idx} == scalar(@{$envt->{tests_file}->{data}}))
  {
    $envt->{test_seq_idx}=0;
    {
      lock($test_counters_lock);
      $test_counters{loop_count}++; 
    }
  }

  return $test;  
}

sub get_test
{
   my $envt=shift;

   {
     lock($test_counters_lock);
     $test_counters{test_count}++;
   }
   
   if ($envt->{mode} eq 'seq')
   {
     return get_next_test($envt);
   }
   elsif ($envt->{mode} eq 'random')
   {
     return get_random_test($envt);
   }
}

sub stress_log
{
  my ($log_file, $line)=@_;
 
  {
    open(SLOG,">>$log_file") or warn "Error during opening log file $log_file";
    print SLOG $line,"\n";
    close(SLOG);
  }
}

sub log_session_errors
{
  my ($env, $test_name) = @_;
  my $line='';

  {
    lock ($log_file_lock);

    #header in the begining of log file
    if (!-e $stress_log_file)
    {
      stress_log($stress_log_file, 
                   "TestID TID      Suite         TestFileName Found Errors");
      stress_log($stress_log_file, 
                   "=======================================================");    
    }

    $line=sprintf('%6d %3d %10s %20s %s', $env->{test_count}, threads->self->tid, 
                                          $opt_suite, $test_name, 
                                          join(",", @{$env->{test_status}}));
                                      
    stress_log($stress_log_file, $line);
    #stress_log_with_lock($stress_log_file, "\n");

    if ($opt_log_error_details)
    {
      foreach $severity (sort {$a cmp $b} keys %{$env->{errors}})
      {
        stress_log($stress_log_file, "");
        foreach $error (keys %{$env->{errors}->{$severity}})
        {
          if ($error ne 'total')
          {
            stress_log($stress_log_file, "$severity: Count:".
                      $env->{errors}->{$severity}->{$error}->[0].
                      " Error:". $env->{errors}->{$severity}->{$error}->[1]);
          }
        }
      }
    }
  }
}

sub sig_INT_handler
{
  $SIG{INT}= \&sig_INT_handler;
  $exiting=1;
  print STDERR "$$: Got INT signal-------------------------------------------\n";

}

sub sig_TERM_handler
{
  $SIG{TERM}= \&sig_TERM_handler;
  $exiting=1;
  print STDERR "$$: Got TERM signal\n";
}

sub usage
{
  my $retcode= shift;
  print <<EOF;

The MySQL Stress suite Ver $stress_suite_version

mysql-stress-test.pl --stress-basedir=<dir> --stress-suite-basedir=<dir> --server-logs-dir=<dir>

--server-host
--server-port
--server-socket
--server-user
--server-password
--server-logs-dir
  Directory where all clients session logs will be stored. Usually 
  this is shared directory associated with server that used 
  in testing

  Required option.

--stress-suite-basedir=<dir>
  Directory that has r/ t/ subfolders with test/result files
  which will be used for testing. Also by default we are looking 
  in this directory for 'stress-tests.txt' file which contains 
  list of tests.  It is possible to specify other location of this 
  file with --stress-tests-file option.

  Required option.

--stress-basedir=<dir>
  Working directory for this test run. This directory will be used 
  as temporary location for results tracking during testing
  
  Required option.

--stress-datadir=<dir>
  Location of data files used which will be used in testing.
  By default we search for these files in <dir>/data where dir 
  is value of --stress-suite-basedir option.

--stress-init-file[=/path/to/file with tests for initialization of stress db]
  Using of this option allows to perform initialization of database
  by execution of test files. List of tests will be taken either from 
  specified file or if it omited from default file 'stress-init.txt'
  located in <--stress-suite-basedir/--suite> dir
    
--stress-tests-file[=/path/to/file with tests] 
  Using of this option allows to run stress test itself. Tests for testing 
  will be taken either from specified file or if it omited from default 
  file 'stress-tests.txt' located in <--stress-suite-basedir/--suite> dir

--stress-mode= [random|seq]
  There are two possible modes which affect order of selecting tests
  from the list:
    - in random mode tests will be selected in random order
    - in seq mode each thread will execute tests in the loop one by one as 
      they specified in the list file. 
      
--sleep-time=<time in seconds>
  Delay between test execution. Could be usefull in continued testsing 
  when one of instance of stress script perform periodical cleanup or
  recreating of some database objects

--threads=#number of threads
  Define number of threads

--check-tests-file
  Check file with list of tests. If file was modified it will force to
  reread list of tests. Could be usefull in continued testing for
  adding/removing tests without script interruption 

--mysqltest=/path/to/mysqltest binary

--verbose

--cleanup
  Force to clean up working directory (specified with --stress-basedir)

--abort-on-error=<number>
  Causes the script to abort if an error with severity <= number was encounterd

--log-error-details
  Enable errors details in the global error log file. (Default: off)

--test-count=<number of executed tests before we have to exit>
--loop-count=<number of executed loops in sequential mode before we have to exit>
--test-duration=<number of seconds that stress test should run>

Example of tool usage:

perl mysql-stress-test.pl \
--stress-suite-basedir=/opt/qa/mysql-test-extra-5.0/mysql-test \
--stress-basedir=/opt/qa/test \
--server-logs-dir=/opt/qa/logs \
--test-count=20  \
--stress-tests-file=innodb-tests.txt \
--stress-init-file=innodb-init.txt \
--threads=5 \
--suite=funcs_1  \
--mysqltest=/opt/mysql/mysql-5.0/client/mysqltest \
--server-user=root \
--server-database=test \
--cleanup \

EOF
exit($retcode);
}


