# ==== Purpose ====
#
# Determine the maximum number of the log file list matched by the provided
# pattern.
#
# ==== Usage ====
#
# --let $pattern_to_match = TEXT
# --source include/rpl_log_file_max_number.inc
#
# Parameters:
#
#   $pattern_to_match
#     File and/or directory pattern matching to be searched for when determining
#     the max number of the log file list.
#
# ==== References ====
#
# BUG#28684376 MAKE GR REPLICATION CHANNELS ROTATE RELAY LOG ON FLUSH LOGS
#

if ($pattern_to_match == '')
{
  --die !!!ERROR IN TEST: you must set $pattern_to_match
}

--let $include_filename = rpl_log_file_max_number.inc
--source include/begin_include_file.inc

--let $result_file1 = `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/max_log_file_', UUID(), '.inc')`
--let $result_file2 = `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/max_log_file_', UUID(), '.inc')`
--let DIR_STATE_MATCH_PATTERN = $pattern_to_match
--let DIR_STATE_RESULT_FILE_1 = $result_file1
--let DIR_STATE_RESULT_FILE_2 = $result_file2

--perl SCRIPT
  my $match_pattern = $ENV{'DIR_STATE_MATCH_PATTERN'};
  my $out1 = $ENV{'DIR_STATE_RESULT_FILE_1'};
  my $out2 = $ENV{'DIR_STATE_RESULT_FILE_2'};
  my @files = glob($match_pattern);
  my $max = 0;
  my $max_ext;

  foreach my $file (@files)
  {
    my @parts = split(/\./, $file);
    my $ext = $parts[scalar(@parts) - 1];
    my $num = $ext + 0;

    if ($num > $max)
    {
      $max = $num;
      $max_ext = $ext;
    }
  }

  open OUT, "> $out1" or die("Unable to open $out: $!\n");
  print OUT $max;
  close OUT;
  open OUT, "> $out2" or die("Unable to open $out: $!\n");
  print OUT $max_ext;
  close OUT;
#END OF
SCRIPT

--let $log_file_max_number = `SELECT LOAD_FILE('$result_file1')`
--remove_file $result_file1

--let $log_file_max_extension = `SELECT LOAD_FILE('$result_file2')`
--remove_file $result_file2

--let $include_filename = rpl_log_file_max_number.inc
--source include/end_include_file.inc
