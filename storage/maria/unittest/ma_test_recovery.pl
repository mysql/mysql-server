#!/usr/bin/env perl

use Getopt::Long;
use File::Copy;
use File::Compare;
use File::Basename;
use Digest::MD5;

$|= 1;
$^W = 1; # warnings, because env cannot parse 'perl -w'
$VER= "1.2";

$opt_version= 0;
$opt_help=    0;
$opt_verbose= 0;
$opt_abort_on_error=0;

my $silent= "-s";
my $maria_path;     # path to "storage/maria"
my $maria_exe_path; # path to executables (ma_test1, aria_chk etc)
my $tmp= "./tmp";
my $my_progname= $0;
my $suffix;
my $zerofilled_tables= 0;

$my_progname=~ s/.*[\/]//;
$maria_path= dirname($0) . "/..";

main();

####
#### main function
####

sub main
{
  my ($res, $table);

  if (!GetOptions("abort-on-error", "help", "version", "verbose"))
  {
    $flag_exit= 1;
  }
  if ($opt_version)
  {
    print "$my_progname version $VER\n";
    exit(0);
  }
  usage() if ($opt_help || $flag_exit);

  $suffix= ( $^O =~ /win/i  && $^O !~ /darwin/i ) ? ".exe" : "";
  $maria_exe_path= "$maria_path/release";
  # we use -f, sometimes -x is unexpectedly false in Cygwin
  if ( ! -f "$maria_exe_path/ma_test1$suffix" )
  {
    $maria_exe_path= "$maria_path/relwithdebinfo";
    if ( ! -f "$maria_exe_path/ma_test1$suffix" )
    {
      $maria_exe_path= "$maria_path/debug";
      if ( ! -f "$maria_exe_path/ma_test1$suffix" )
      {
        $maria_exe_path= $maria_path;
        if ( ! -f "$maria_exe_path/ma_test1$suffix" )
        {
          die("Cannot find ma_test1 executable\n");
        }
      }
    }
  }

  # test data is always put in the current directory or a tmp subdirectory
  # of it

  if (! -d "$tmp")
  {
    mkdir $tmp;
  }
  print "ARIA RECOVERY TESTS\n";

  # To not flood the screen, we redirect all the commands below to a text file
  # and just give a final error if their output is not as expected

  open (MY_LOG, ">$tmp/ma_test_recovery.output") or die "Can't open log file\n";
  print MY_LOG "Testing the REDO PHASE ALONE\n";

  # runs a program inserting/deleting rows, then moves the resulting table
  # elsewhere; applies the log and checks that the data file is
  # identical to the saved original.

  my @t= ("ma_test1$suffix $silent -M -T -c",
          "ma_test2$suffix $silent -L -K -W -P -M -T -c -d500",
          "ma_test2$suffix $silent -M -T -c -b65000",
          "ma_test2$suffix $silent -M -T -c -b65000 -d800",
          "ma_test1$suffix $silent -M -T -c -C",
          "ma_test2$suffix $silent -L -K -W -P -M -T -c -d500 -C",
          #"ma_rt_test$suffix $silent -M -T -c -C",
          # @todo: also add to @t2
         );

  foreach my $prog (@t)
  {
    unlink <aria_log.* aria_log_control>;
    my $prog_no_suffix= $prog;
    $prog_no_suffix=~ s/$suffix// if ($suffix);
    print MY_LOG "TEST WITH $prog_no_suffix\n";
    $res= my_exec("$maria_exe_path/$prog");
    print MY_LOG $res;
    # derive table's name from program's name
    if ($prog =~ m/^ma_(\S+)\s.*/)
    {
      $table= $1;
    }
    else
    {
      die("can't guess table name");
    }
    $com=  "$maria_exe_path/aria_chk$suffix -dvv $table ";
    $com.= "| grep -v \"Creation time:\" | grep -v \"recover time:\" | grep -v \"file length\" | grep -v \"LSNs:\" | grep -v \"UUID:\"";
    $com.= "> $tmp/aria_chk_message.good.txt 2>&1";
    my_exec($com);
    my $checksum= my_exec("$maria_exe_path/aria_chk$suffix -dss $table");
    move("$table.MAD", "$tmp/$table-good.MAD") ||
      die "Can't move $table.MAD to $tmp/$table-good.MAD\n";
    move("$table.MAI", "$tmp/$table-good.MAI") ||
      die "Can't move $table.MAI to $tmp/$table-good.MAI\n";
    apply_log($table, "shouldnotchangelog");
    check_table_is_same($table, $checksum);
    $res= physical_cmp($table, "$tmp/$table-good");
    print MY_LOG $res;
    print MY_LOG "testing idempotency\n";
    apply_log($table, "shouldnotchangelog");
    check_table_is_same($table, $checksum);
    $res= physical_cmp($table, "$tmp/$table-good");
    print MY_LOG $res;
  }

  print MY_LOG "Testing the REDO AND UNDO PHASE\n";
  # The test programs look like:
  # work; commit (time T1); work; exit-without-commit (time T2)
  # We first run the test program and let it exit after T1's commit.
  # Then we run it again and let it exit at T2. Then we compare
  # and expect identity.

  my @take_checkpoints= ("no", "yes");
  my @blobs= ("", "-b32768");
  my @test_undo= (1, 2, 3, 4);
  my @t2= ("ma_test1$suffix $silent -M -T -c -N blob -H1",
           "--testflag=1",
           "--testflag=2 --test-undo=",
           "ma_test1$suffix $silent -M -T -c -N blob -H2",
           "--testflag=3",
           "--testflag=4 --test-undo=",
           "ma_test1$suffix $silent -M -T -c -N blob -H2 --versioning",
           "--testflag=3",
           "--testflag=4 --test-undo=",
           "ma_test1$suffix $silent -M -T -c -N blob -H2",
           "--testflag=2",
           "--testflag=3 --test-undo=",
           "ma_test2$suffix $silent -L -K -W -P -M -T -c blob -H1",
           "-t1",
           "-t2 -A",
           "ma_test2$suffix $silent -L -K -W -P -M -T -c blob -H1",
           "-t1",
           "-t6 -A");

  foreach my $take_checkpoint (@take_checkpoints)
  {
    my ($i, $j, $k, $commit_run_args, $abort_run_args);
    # we test table without blobs and then table with blobs
    for ($i= 0; defined($blobs[$i]); $i++)
    {
      for ($j= 0; defined($test_undo[$j]); $j++)
      {
        # first iteration tests rollback of insert, second tests rollback of delete
        # -N (create NULL fields) is needed because --test-undo adds it anyway
        for ($k= 0; defined($t2[$k]); $k+= 3)
        {
          $prog= $t2[$k];
          $prog=~ s/blob/$blobs[$i]/;
          if ("$take_checkpoint" eq "no") {
            $prog=~ s/\s+\-H[0-9]+//;
          }
          $commit_run_args= $t2[$k + 1];
          $abort_run_args= $t2[$k + 2];
          unlink <aria_log.* aria_log_control>;
          my $prog_no_suffix= $prog;
          $prog_no_suffix=~ s/$suffix// if ($suffix);
          print MY_LOG "TEST WITH $prog_no_suffix $commit_run_args (commit at end)\n";
          $res= my_exec("$maria_exe_path/$prog $commit_run_args");
          print MY_LOG $res;
          # derive table's name from program's name
          if ($prog =~ m/^ma_(\S+)\s.*/)
          {
            $table= $1;
          }
          else
          {
            die("can't guess table name");
          }
          $com=  "$maria_exe_path/aria_chk$suffix -dvv $table ";
          $com.= "| grep -v \"Creation time:\" | grep -v \"recover time:\" | grep -v \"recover time:\" |grep -v \"file length\" | grep -v \"LSNs:\" | grep -v \"UUID:\" ";
          $com.= "> $tmp/aria_chk_message.good.txt 2>&1";
          $res= my_exec($com);
          print MY_LOG $res;
          $checksum= my_exec("$maria_exe_path/aria_chk$suffix -dss $table");
          move("$table.MAD", "$tmp/$table-good.MAD") ||
            die "Can't move $table.MAD to $tmp/$table-good.MAD\n";
          move("$table.MAI", "$tmp/$table-good.MAI") ||
            die "Can't move $table.MAI to $tmp/$table-good.MAI\n";
          unlink <aria_log.* aria_log_control>;
          print MY_LOG "TEST WITH $prog_no_suffix $abort_run_args$test_undo[$j] (additional aborted work)\n";
          $res= my_exec("$maria_exe_path/$prog $abort_run_args$test_undo[$j]");
          print MY_LOG $res;
          copy("$table.MAD", "$tmp/$table-before_undo.MAD") ||
            die "Can't copy $table.MAD to $tmp/$table-before_undo.MAD\n";
          copy("$table.MAI", "$tmp/$table-before_undo.MAI") ||
            die "Can't copy $table.MAI to $tmp/$table-before_undo.MAI\n";

          # The lines below seem unneeded, will be removed soon
          # We have to copy and restore logs, as running aria_read_log will
          # change the aria_control_file
          #    rm -f $tmp/aria_log.* $tmp/aria_log_control
          #    cp $maria_path/aria_log* $tmp

          if ($test_undo[$j] != 3) {
            apply_log($table, "shouldchangelog"); # should undo aborted work
          } else {
            # probably nothing to undo went to log or data file
            apply_log($table, "dontknow");
          }
          copy("$table.MAD", "$tmp/$table-after_undo.MAD") ||
            die "Can't copy $table.MAD to $tmp/$table-after_undo.MAD\n";
          copy("$table.MAI", "$tmp/$table-after_undo.MAI") ||
            die "Can't copy $table.MAI to $tmp/$table-after_undo.MAI\n";

          # It is impossible to do a "cmp" between .good and .after_undo,
          # because the UNDO phase generated log
          # records whose LSN tagged pages. Another reason is that rolling back
          # INSERT only marks the rows free, does not empty them (optimization), so
          # traces of the INSERT+rollback remain.

          check_table_is_same($table, $checksum);
          print MY_LOG "testing idempotency\n";
          apply_log($table, "shouldnotchangelog");
          check_table_is_same($table, $checksum);
          $res= physical_cmp($table, "$tmp/$table-after_undo");
          print MY_LOG $res;
          print MY_LOG "testing applying of CLRs to recreate table\n";
          unlink <$table.MA?>;
          #    cp $tmp/aria_log* $maria_path  #unneeded
          apply_log($table, "shouldnotchangelog");
          check_table_is_same($table, $checksum);
          $res= physical_cmp($table, "$tmp/$table-after_undo");
          print MY_LOG $res;
        }
        unlink <$table.* $tmp/$table* $tmp/aria_chk_*.txt $tmp/aria_read_log_$table.txt>;
      }
    }
  }

  if ($? >> 8) {
    print "Some test failed\n";
    exit(1);
  }

  close(MY_LOG);
  # also note that aria_chk -dvv shows differences for ma_test2 in UNDO phase,
  # this is normal: removing records does not shrink the data/key file,
  # does not put back the "analyzed,optimized keys"(etc) index state.
  `diff -b $maria_path/unittest/ma_test_recovery.expected $tmp/ma_test_recovery.output`;
  if ($? >> 8) {
    print "UNEXPECTED OUTPUT OF TESTS, FAILED";
    print " (zerofilled $zerofilled_tables tables)\n";
    print "For more info, do diff -b $maria_path/unittest/ma_test_recovery.expected ";
    print "$tmp/ma_test_recovery.output\n";
    exit(1);
  }
  print "ALL RECOVERY TESTS OK (zerofilled $zerofilled_tables tables)\n";
}

####
#### check_table_is_same
####

sub check_table_is_same
{
  my ($table, $checksum)= @_;
  my ($com, $checksum2, $res);

  # Computes checksum of new table and compares to checksum of old table
  # Shows any difference in table's state (info from the index's header)
  # Data/key file length is random in ma_test2 (as it uses srand() which
  # may differ between machines).

  if ($opt_verbose)
  {
    print "checking if table $table has changed\n";
  }

  $com=  "$maria_exe_path/aria_chk$suffix -dvv $table | grep -v \"Creation time:\" | grep -v \"recover time:\"";
  $com.= "| grep -v \"file length\" | grep -v \"LSNs:\" | grep -v \"UUID:\" > $tmp/aria_chk_message.txt 2>&1";
  $res= `$com`;
  print MY_LOG $res;
  $res= `$maria_exe_path/aria_chk$suffix -ss -e --read-only $table`;
  print MY_LOG $res;
  $checksum2= `$maria_exe_path/aria_chk$suffix -dss $table`;
  if ("$checksum" ne "$checksum2")
  {
    print MY_LOG "checksum differs for $table before and after recovery\n";
    return 1;
  }

  $com=  "diff $tmp/aria_chk_message.good.txt $tmp/aria_chk_message.txt ";
  $com.= "> $tmp/aria_chk_diff.txt || true";
  $res= `$com`;
  print MY_LOG $res;

  if (-s "$tmp/aria_chk_diff.txt")
  {
    print MY_LOG "Differences in aria_chk -dvv, recovery not yet perfect !\n";
    print MY_LOG "========DIFF START=======\n";
    open(MY_FILE, "<$tmp/aria_chk_diff.txt") || die "Can't open file aria_chk_diff.txt\n";
    while (<MY_FILE>)
    {
      print MY_LOG $_;
    }
    close(MY_FILE);
    print MY_LOG "========DIFF END=======\n";
  }
}

####
#### apply_log
####

sub apply_log
{
  my ($table, $shouldchangelog)= @_;
  my ($log_md5, $log_md5_2);

  # applies log, can verify if applying did write to log or not

  if ("$shouldchangelog" ne "shouldnotchangelog" &&
      "$shouldchangelog" ne "shouldchangelog" &&
      "$shouldchangelog" ne "dontknow" )
  {
    print MY_LOG "bad argument '$shouldchangelog'\n";
    return 1;
  }
  foreach (<aria_log.*>)
  {
    $log_md5.= md5_conv($_);
  }
  print MY_LOG "applying log\n";
  my_exec("$maria_exe_path/aria_read_log$suffix -a > $tmp/aria_read_log_$table.txt");
  foreach (<aria_log.*>)
  {
    $log_md5_2.= md5_conv($_);
  }
  if ("$log_md5" ne "$log_md5_2" )
  {
    if ("$shouldchangelog" eq "shouldnotchangelog")
    {
      print MY_LOG "aria_read_log should not have modified the log\n";
      return 1;
    }
  }
  elsif ("$shouldchangelog" eq "shouldchangelog")
  {
    print MY_LOG "aria_read_log should have modified the log\n";
    return 1;
  }
}

####
#### md5_conv
####

sub md5_conv
{
  my ($file)= @_;

  open(FILE, $file) or die "Can't open '$file': $!\n";
  binmode(FILE);
  my $md5= Digest::MD5->new;
  $md5->addfile(FILE);
  close (FILE);
  return $md5->hexdigest . "\n";
}

####
#### physical_cmp: compares two tables (MAI and MAD) physically;
#### uses zerofill-keep-lsn to reduce irrelevant differences.
####

sub physical_cmp
{
  my ($table1, $table2)= @_;
  my ($zerofilled, $ret_text)= (0, "");
  #return `cmp $table1.MAD $table2.MAD`.`cmp $table1.MAI $table2.MAI`;
  foreach my $file_suffix ("MAD", "MAI")
  {
    my $file1= "$table1.$file_suffix";
    my $file2= "$table2.$file_suffix";
    my $res= File::Compare::compare($file1, $file2);
    die() if ($res == -1);
    if ($res == 1 # they differ
        and !$zerofilled)
    {
      # let's try with --zerofill-keep-lsn
      $zerofilled= 1; # but no need to do it twice
      $zerofilled_tables= $zerofilled_tables + 1;
      my $table_no= 1;
      foreach my $table ($table1, $table2)
      {
        # save original tables to restore them later
        copy("$table.MAD", "$tmp/before_zerofill$table_no.MAD") || die();
        copy("$table.MAI", "$tmp/before_zerofill$table_no.MAI") || die();
        $com= "$maria_exe_path/aria_chk$suffix -ss --zerofill-keep-lsn --skip-update-state $table";
        $res= `$com`;
        print MY_LOG $res;
        $table_no= $table_no + 1;
      }
      $res= File::Compare::compare($file1, $file2);
      die() if ($res == -1);
    }
    $ret_text.= "$file1 and $file2 differ\n" if ($res != 0);
  }
  if ($zerofilled)
  {
    my $table_no= 1;
    foreach my $table ($table1, $table2)
    {
      move("$tmp/before_zerofill$table_no.MAD", "$table.MAD") || die();
      move("$tmp/before_zerofill$table_no.MAI", "$table.MAI") || die();
      $table_no= $table_no + 1;
    }
  }
  return $ret_text;
}


sub my_exec
{
  my($command)= @_;
  my $res;
  if ($opt_verbose)
  {
    print "$command\n";
  }
  $res= `$command`;
  if ($? != 0 && $opt_abort_on_error)
  {
    exit(1);
  }
  return $res;
}


####
#### usage
####

sub usage
{
  print <<EOF;
$my_progname version $VER

Description:

Run various Aria recovery tests and print the results

Options
--help             Show this help and exit.

--abort-on-error   Abort at once in case of error.
--verbose          Show commands while there are executing.
--version          Show version number and exit.

EOF
  exit(0);
}
