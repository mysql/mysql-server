#!/usr/bin/env perl


use strict;
use warnings;

my $usage= <<EOF;
This program tests that the options
--aria-force-start-after-recovery-failures --aria-recover work as
expected.
It has to be run from directory mysql-test, and works with non-debug
and debug binaries.
Pass it option -d or -i (to test corruption of data or index file).
EOF

# -d currently exhibits BUG#36578
# "Maria: maria-recover may fail to autorepair a table"

die($usage) if (@ARGV == 0);

my $corrupt_index;

if ($ARGV[0] eq '-d')
  {
    $corrupt_index= 0;
  }
elsif ($ARGV[0] eq '-i')
  {
    $corrupt_index= 1;
  }
else
  {
    die($usage);
  }

my $force_after= 3;
my $corrupt_file= $corrupt_index ? "MAI" : "MAD";
my $corrupt_message= 
  "\\[ERROR\\] mysqld(.exe)*: Table '..test.t1' is marked as crashed and should be repaired";

my $sql_name= "./var/tmp/create_table.sql";
my $error_log_name= "./var/log/master.err";
my @cmd_output;
my $whatever; # garbage data
$ENV{MTR_VERSION} = 1; # MTR2 does not have --start-and-exit
my $base_server_cmd= "perl mysql-test-run.pl --mysqld=--aria-force-start-after-recovery-failures=$force_after --suite=maria maria.maria-recover ";
if ($^O =~ /^mswin/i)
  {
    print <<EOF;
WARNING: with Activestate Perl, mysql-test-run.pl --start-and-exit has a bug:
it does not exit; cygwin perl recommended
EOF
  }
my $iswindows= ( $^O =~ /win/i  && $^O !~ /darwin/i );
$base_server_cmd.= ($iswindows ? "--mysqld=--console" : "--mem");
my $server_cmd;
my $server_pid_name="./var/run/master.pid";
my $server_pid;
my $i; # count of server restarts
sub kill_server;

my $suffix= ($iswindows ? ".exe" : "");
my $client_exe_path= "../client/release";
# we use -f, sometimes -x is unexpectedly false in Cygwin
if ( ! -f "$client_exe_path/mysql$suffix" )
  {
    $client_exe_path= "../client/relwithdebinfo";
    if ( ! -f "$client_exe_path/mysql$suffix" )
    {
      $client_exe_path= "../client/debug";
      if ( ! -f "$client_exe_path/mysql$suffix" )
      {
        $client_exe_path= "../client";
        if ( ! -f "$client_exe_path/mysql$suffix" )
        {
          die("Cannot find 'mysql' executable\n");
        }
      }
    }
  }

print "starting mysqld\n";
$server_cmd= $base_server_cmd . " --start-and-exit 2>&1";
@cmd_output=`$server_cmd`;
die if $?;
my $master_port= (grep (/Using MASTER_MYPORT .*= (\d+)$/, @cmd_output))[0];
$master_port =~ s/.*= //;
chomp $master_port;
die unless $master_port > 0;

my $client_cmd= "$client_exe_path/mysql -u root -h 127.0.0.1 -P $master_port test < $sql_name";

open(FILE, ">", $sql_name) or die;

# To exhibit BUG#36578 with -d, we don't create an index if -d. This is
# because the presence of an index will cause repair-by-sort to be used,
# where sort_get_next_record() is only called inside
#_ma_create_index_by_sort(), so the latter function fails and in this
# case retry_repair is set, so bug does not happen. Whereas without
# an index, repair-with-key-cache is called, which calls
# sort_get_next_record() whose failure itself does not cause a retry.

print FILE "create table t1 (a varchar(1000)".
  ($corrupt_index ? ", index(a)" : "") .") engine=aria;\n";
print FILE <<EOF;
insert into t1 values("ThursdayMorningsMarket");
# If Recovery executes REDO_INDEX_NEW_PAGE it will overwrite our
# intentional corruption; we make Recovery skip this record by bumping
# create_rename_lsn using OPTIMIZE TABLE. This also makes sure to put
# the pages on disk, so that we can corrupt them.
optimize table t1;
# mark table open, so that --aria-recover repairs it
insert into t1 select concat(a,'b') from t1 limit 1;
EOF
close FILE;

print "creating table\n";
`$client_cmd`;
die if $?;

print "killing mysqld hard\n";
kill_server(9);

print "ruining " .
  ($corrupt_index ? "first page of keys" : "bitmap page") .
  " in table to test aria-recover\n";
open(FILE, "+<", "./var/master-data/test/t1.$corrupt_file") or die;
$whatever= ("\xAB" x 100);
sysseek (FILE, $corrupt_index ? 8192 : (8192-100-100), 0) or die;
syswrite (FILE, $whatever) or die;
close FILE;

print "ruining log to make recovery fail; mysqld should fail the $force_after first restarts\n";
open(FILE, "+<", "./var/tmp/aria_log.00000001") or die;
$whatever= ("\xAB" x 8192);
sysseek (FILE, 99, 0) or die;
syswrite (FILE, $whatever) or die;
close FILE;

$server_cmd= $base_server_cmd . " --start-dirty 2>&1";
for($i= 1; $i <= $force_after; $i= $i + 1)
  {
    print "mysqld restart number $i... ";
    unlink($error_log_name) or die;
    `$server_cmd`;
    # mysqld should return 1 when can't read log
    die unless (($? >> 8) == 1);
    open(FILE, "<", $error_log_name) or die;
    @cmd_output= <FILE>;
    close FILE;
    die unless grep(/\[ERROR\] mysqld(.exe)*: Aria engine: log initialization failed/, @cmd_output);
    die unless grep(/\[ERROR\] Plugin 'Aria' init function returned error./, @cmd_output);
    print "failed - ok\n";
  }

print "mysqld restart number $i... ";
unlink($error_log_name) or die;
@cmd_output=`$server_cmd`;
die if $?;
open(FILE, "<", $error_log_name) or die;
@cmd_output= <FILE>;
close FILE;
die unless grep(/\[Warning\] mysqld(.exe)*: Aria engine: removed all logs after [\d]+ consecutive failures of recovery from logs/, @cmd_output);
die unless grep(/\[ERROR\] mysqld(.exe)*: File '.*tmp.aria_log.00000001' not found \(Errcode: 2\)/, @cmd_output);
print "success - ok\n";

open(FILE, ">", $sql_name) or die;
print FILE <<EOF;
set global aria_recover=normal;
insert into t1 values('aaa');
EOF
close FILE;

# verify corruption has not yet been noticed
open(FILE, "<", $error_log_name) or die;
@cmd_output= <FILE>;
close FILE;
die if grep(/$corrupt_message/, @cmd_output);

print "inserting in table\n";
`$client_cmd`;
die if $?;
print "table is usable - ok\n";

open(FILE, "<", $error_log_name) or die;
@cmd_output= <FILE>;
close FILE;
die unless grep(/$corrupt_message/, @cmd_output);
die unless grep(/\[Warning\] Recovering table: '..test.t1'/, @cmd_output);
print "was corrupted and automatically repaired - ok\n";

# remove our traces
kill_server(15);

print "TEST ALL OK\n";

# kills mysqld with signal given in parameter
sub kill_server
  {
    my ($sig)= @_;
    my $wait_count= 0;
    my $kill_cmd;
    my @kill_output;
    open(FILE, "<", $server_pid_name) or die;
    @cmd_output= <FILE>;
    close FILE;
    $server_pid= $cmd_output[0];
    chomp $server_pid;
    die unless $server_pid > 0;
    if ($iswindows)
      {
        # On Windows, server_pid_name is not the "main" process id
        # so perl's kill() does not see this process id.
        # But taskkill works, though only with /F ("-9"-style kill).
        $kill_cmd= "taskkill /F /PID $server_pid 2>&1";
        @kill_output= `$kill_cmd`;
        die unless grep(/has been terminated/, @kill_output);
      }
    else
      {
        kill($sig, $server_pid) or die;
      }
    while (1) # wait until mysqld process gone
      {
        if ($iswindows)
          {
            @kill_output= `$kill_cmd`;
            last if grep(/not found/, @kill_output);
          }
        else
          {
            kill (0, $server_pid) or last;
          }
        print "waiting for mysqld to die\n" if ($wait_count > 30);
        $wait_count= $wait_count + 1;
        select(undef, undef, undef, 0.1);
      }
  }
