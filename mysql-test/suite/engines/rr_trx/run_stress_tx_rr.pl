#!/usr/bin/perl
################################################################################
#
# This script runs the transactional stress test "stress_tx_rr" against the
# transactional storage engine and looks for errors in two log files:
#   var/stress/<timestamp>/mysql-stress-test.log
#   var/log/master.err
#
# The script assumes current working dir is mysql-test/.
#
# Regarding the server error log, currently only error lines containing the 
# string "Error:" will be reported as a critical error, in addition to signs
# of crashes.
#
# In the stress test log, all lines matching the regex "S\d:" (denoting an
# error with a specified severity) will be reported as errors.
#
# Error information including the full server log in the case of server crash
# is output to standard out.
#
# This script is and should be silent if no errors are detected.
#
################################################################################

use File::Find;
use File::Spec;
use Cwd;
use Cwd 'abs_path';
use Getopt::Long;

# Checking script is run from the correct location
if (! -f "mysql-test-run.pl") {
   print("\nERROR: This script should be run from the \'\<INSTALL_DIR\>/mysql-test\' directory.\n");
   error(1);
}

$runlog="rr_trx.log";

my $errorFound;

my $installdir=abs_path(File::Spec->updir());

my $f=abs_path($0);
my ($v,$d,$f)=File::Spec->splitpath($f);
my $testsuitedir=$v.$d;

################################################################################
# Run stress test, redirect output to tmp file.
# Duration is specified in seconds. Some nice values:
#   5 minutes =   300 
#  30 minutes =  1800
#   1 hour    =  3600
#   2 hours   =  7200
#   5 hours   = 18000
#  12 hours   = 43200
#
################################################################################
$opt_duration=600;

# Special handling for the InnoDB plugin
$plugin_params="\"--plugin-load=innodb=ha_innodb_plugin.so;innodb_trx=ha_innodb_plugin.so;innodb_locks=ha_innodb_plugin.so;innodb_cmp=ha_innodb_plugin.so;innodb_cmp_reset=ha_innodb_plugin.so;innodb_cmpmem=ha_innodb_plugin.so;innodb_cmpmem_reset=ha_innodb_plugin.so\"";
$plugin_params=~s/so/dll/g if (windows());

$opt_help="";
$opt_try="";
$opt_engine="";
$opt_threads=10;

# Collection command line options
GetOptions("engine:s"   => \$opt_engine, 
           "duration=i" => \$opt_duration, 
           "threads=i"  => \$opt_treads, 
           "try", "help") || usage();
 
if ($opt_help) { usage(); }
if (!$opt_engine) {
   print("\nERROR: --engine=\<engine\> argument is required!!!\n");
   usage();
}


# setting specific engine parameters
$engine_options="";
  # for innodb engine
  if ($opt_engine eq "InnoDB") {
     $engine_options=
           "--mysqld=--innodb " .
           "--mysqld=--innodb-lock-wait-timeout=2 " .
           " ";
     }
  elsif ($opt_engine eq "InnoDB_plugin") {  
        $engine_options=
           "--mysqld=--innodb " .
           "--mysqld=--ignore-builtin-innodb " . 
           #"--mysqld=--plugin_dir=".$installdir."/lib " .
           "--mysqld=--plugin_dir=".$installdir."/storage/innodb_plugin/.libs " .
           "--mysqld=--innodb-lock-wait-timeout=2 " .
           "--mysqld=".$plugin_params." " .
           " ";
     }
  # add parameters for a new engine by modifying the 'elsif' section below
  elsif ($opt_engine eq "zz") {  
        $engine_options=
           " ";
     }
  else  { 
        print("\nERROR: '".$opt_engine."' - unknown engine\n");
        add_engine_help(); 
     }

# From this point forward there is no difference between the build in InnDB and the plugin
$opt_engine='InnoDB' if ($opt_engine eq 'InnoDB_plugin');

# checking that custom files for that engine exist
$engine_lower= lc($opt_engine);
$missing=0;
if (!-f $testsuitedir.'init_'.$engine_lower.'.txt') { 
   print("\nERROR: config file 'init_".$engine_lower.".txt' missing."); 
   $missing=1;
}
if (!-f $testsuitedir.'t/init_'.$engine_lower.'.test') { 
   print("\nERROR: config file 'init_".$engine_lower.".test' missing."); 
   $missing=1;
}
if (!-f $testsuitedir.'r/init_'.$engine_lower.'.result') {
   print("\nERROR: config file 'init_".$engine_lower.".result' missing."); 
   $missing=1;
}
add_engine_help() if ($missing); 

# bilding test command line
$cmd="MTR_VERSION=1 " .
    "perl ./mysql-test-run.pl " .
    "--comment=stress_tx_rr_".$opt_engine." " .
    "--stress " .
    "--stress-init-file=init_".$engine_lower.".txt " .
    "--stress-test-file=run.txt " .
    "--stress-suite=engines/rr_trx " .
    "--stress-test-duration=".$opt_duration." " .
    "--stress-threads=".$opt_threads."  " .
    "--mysqld=--log-output=file " .
    "--mysqld=--sql-mode=no_engine_substitution " .
    "--skip-im " .
    "--skip-ndb " .
    $engine_options . 
    " > ".$runlog." 2>&1";

# running the test
print("\n   Running \'rr_trx\' test with ".$opt_threads." clients\n");
print("   for ".$opt_duration." seconds using the ".$opt_engine." storag engine.\n");
print("\n   Log file: ".$runlog."\n");
if ($opt_try) {
   print("\nThe following command will execute:\n");
   print("$cmd\n\n");
   exit(0);
}
system $cmd;

################################################################################
# Check for crash and other severe errors in the server log.
#
################################################################################

# Open log file. If MTR_VERSION=1 this is in var/log/master.err.
# Otherwise, it is in ?... [stress_tx_rr not yet runnable with MTR_VERSION=2]
# Assuming current directory mysql-test/
my $serverlog=getcwd() . "/var/log/master.err";

open(SERVERLOG, $serverlog) 
    or die "Unable to open $serverlog. Test not run?";
my @servererrors = ();  # Lines with "Severe" errors in server error log
my @crash = ();    # Empty if no stack trace detected, non-empty otherwise.

# Grep for errors and crashes. Going line-by-line since the file can be large.
while (<SERVERLOG>) {
    $line = $_;
    push @crash, $line if /This could be because you hit a bug/;
    push @servererrors, $line if /Error:/;
}
close(SERVERLOG);

if (@crash) {
    # Crash (stack trace) detected in server log.
    print "Transactional stress test stress_tx_rr:\n\n";
    print "SERVER CRASH DETECTED!\n";
    print "Server log: $serverlog printed at the bottom of this log.\n\n";
    print "########################################################\n\n";
}
if (@servererrors) {
    # "Severe" errors detected. Print error lines to std out
    print "CRITICAL ERRORS:\n\n";
    foreach $error (@servererrors) {
        print $error;
    }
    print "\n########################################################\n\n";
}


################################################################################
# Check for errors reported by mysql-stress-test.pl. Transactional consistency
# issues are shown as result diffs.
################################################################################

my $dir;
find(\&finddir, cwd);  # sets variable $dir

# Open log file
my $logfile="$dir/mysql-stress-test.log";
open(LOGFILE, $logfile) 
    or die "Unable to open $logfile. Test not run?";
my @errors = ();
my @heading = ();

# Grep for errors. Going line-by-line since the file can be large.
while (<LOGFILE>) {
    #push @errors, $_ if ! /No Errors/;
    push @errors, $_ if /S\d:/;
    push @heading, $_ if /TestID|=====/;
}
close(LOGFILE);

# Print all errors, i.e. all lines that do not contain the string "No Errors"
if (@errors) {
    $errorFound = 1;
    print "Stress test main log file: $logfile\n";
    print "Errors follow:\n\n";
    # First print the heading
    foreach $header_line (@heading) {
        print $header_line;
    }
    foreach $error (@errors) {
        print $error;
    }
}


# If errors in server log, output the log and exit 1?
if (@servererrors or @crash) {
    $errorFound = 1;
    print "\n########################################################\n\n";
    print "Server error log (master.err):\n\n";
    
    open(SERVERLOG, $serverlog) 
        or die "Unable to open $serverlog!";
    
    while (<SERVERLOG>) {
        print $_;
    }
    close(SERVERLOG);
}

if ($errorFound) {
    # Exit with error code != 0 if we found an error.
    print("\nTest Completed with errors. \n");
    print(" - See ".$runlog." for summary.\n");
    print(" - See files under var/stress for details.\n");
    exit 1;
}

print("\nTest Completed - See ".$runlog." for details\n");
################################################################################
# Helper routines etc.
#
################################################################################

sub finddir {                       
  my $file = $File::Find::name;       # complete path to the file

  return unless -d $file;             # process directories (-d), not files (-f)
  return unless $_ =~ m/^\d{14}$/;    # check if file matches timstamp regex,
                                      # must be 14 digits
  $dir=$file;
  #$dir= $_;                          # $_ = just the file name, no path
  return $_;
}


sub usage
{
  print <<EOF;

SYNTAX $0 --engine=<engine> [--duration=<nn>] [--thread=<nn>] [--try]

  --engine=<engine>
    The engine used to run the test. \<engine\> needs to be provided exactly as 
    it is reprted in the SHOW ENGINES comand.
    EXCEPTION: In order to use the InnoDB plugin, specify 'InnoDB_plugin'
  Required option.

  --duration=nn
    The time the test should run for in seconds. Defaut value is 600 seconds (10 minutes).
  Optional parameter

  --threads=nn
    The number of clients used by the test driver. Defaut value is 10.
  Optional parameter

  --try
    Do not run the actual test but show what will be run
  Optional parameter


EOF

exit(0);
}

sub add_engine_help
{
  print <<EOF;

\nThis test is can be run against any transactional engine. However scripts need to be modifed in order
to support such engines (support to InnoDB is provided as an example).
In order to add support for a new engine, you will need to modify scripts as follows:
   1) cd to INSTALL_DIR/mysql-test/suite/engines/rr_trx
   2) Modify the 'run_stress_rr.pl' file by adding an 'elsif' section for your engine and have it
      include specifc values required to be passed as startup parameters to the MySQL server by
      specifying them using "--mysqld" options (see InnoDB example).
   3) Copy the 'init_innodb.txt' file to 'init_<engine>.txt file and change its content to be "init_<engine>".
   4) In the 't' directory copy the "init_innodb.test" file to "init_\<engine\>.test" and change the value of
      the '\$engine' variable to \<engine\>.
   5) In the 'r' directory copy "the init_innodb.result" file to "init_\<engine\>.result" and change refrences
      to 'InnoDB' to \<engine\>.

EOF

exit(0);
}

sub windows {
        if (
                ($^O eq 'MSWin32') ||
                ($^O eq 'MSWin64')
        ) {
                return 1;
        } else {
                return 0;
        }
}

