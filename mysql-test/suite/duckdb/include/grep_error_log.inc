# grep_error_log.inc — Extract DuckDB diagnostic log entries from error log.
#
# Usage:
#   --let $log_file = $MYSQLTEST_VARDIR/log/mysqld.1.err
#   --let $grep_pattern = DELETE FROM
#   [--let $grep_exclude = alibaba_rds_row_no]
#   [--let $truncate_log = 1]
#   --source suite/duckdb/include/grep_error_log.inc
#
# Prints matching lines with the "... my_duckdb: " prefix stripped.
# If $truncate_log is set to 1, empties the log file afterwards.

--let _GEL_FILE = $log_file
--let _GEL_PATTERN = $grep_pattern
--let _GEL_EXCLUDE = $grep_exclude
--let _GEL_TRUNCATE = $truncate_log

--perl
  use strict;
  use warnings;
  my $log     = $ENV{'_GEL_FILE'};
  my $pattern = $ENV{'_GEL_PATTERN'};
  my $exclude = $ENV{'_GEL_EXCLUDE'} || '';
  my $trunc   = $ENV{'_GEL_TRUNCATE'} || '0';

  if (open(my $fh, '<', $log)) {
    while (<$fh>) {
      next unless /$pattern/;
      next if $exclude ne '' && /$exclude/;
      next unless s/.*my_duckdb: //;
      print;
    }
    close $fh;
  }

  if ($trunc eq '1' && open(my $wfh, '>', $log)) {
    close $wfh;
  }
EOF

--let $grep_exclude =
--let $truncate_log =
