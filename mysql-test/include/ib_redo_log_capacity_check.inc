# Checks if total size of all redo log files (including _tmp files) does not exceed
# the provided physical capacity (MYSQLD_IB_REDO_LOG_PHYSICAL_CAPACITY). Expects to
# find existing redo files (crashes if total size of redo files is zero).

perl;
require 'include/ib_redo_log_files.inc';

my $physical_capacity = $ENV{'MYSQLD_IB_REDO_LOG_PHYSICAL_CAPACITY'};

print "Checking if total size of redo log files does not exceed $physical_capacity bytes\n";

my $size = 0;
foreach (log_get_all_files()) {
  $size += -s $_;
}
$size <= $physical_capacity or die "Redo log files occupy too much space ($size > $physical_capacity).";
$size > 0 or die "No redo log files have been found.";

EOF
