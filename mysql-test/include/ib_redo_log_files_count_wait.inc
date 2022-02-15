perl;
require 'include/ib_redo_log_files.inc';

my $non_tmp_min = $ENV{'MYSQLD_IB_REDO_LOG_NON_TMP_FILES_COUNT_MIN'};
my $non_tmp_max = $ENV{'MYSQLD_IB_REDO_LOG_NON_TMP_FILES_COUNT_MAX'};

if ((defined $non_tmp_max) || (defined $non_tmp_min)) {
  while (true) {
    my $non_tmp_n = scalar(log_get_non_tmp_files());
    if ((defined $non_tmp_max) && $non_tmp_n <= $non_tmp_max) {
      print "There are <= $non_tmp_max non-temporary redo log files.\n";
      last;
    }
    if ((defined $non_tmp_min) && $non_tmp_min <= $non_tmp_n) {
      print "There are >= $non_tmp_min non-temporary redo log files.\n";
      last;
    }

    sleep 1;
  }
}

my $all_max = $ENV{'MYSQLD_IB_REDO_LOG_ALL_FILES_COUNT_MAX'};
if (defined $all_max) {
  while (true) {
    my $all_n = scalar(log_get_all_files());
    if ($all_n <= $all_max) {
      print "There are <= $all_max redo log files.\n";
      last;
    }
    sleep 1;
  }
}

EOF