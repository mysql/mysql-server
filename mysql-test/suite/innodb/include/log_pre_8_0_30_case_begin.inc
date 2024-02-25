# NOTE: We refuse to upgrade from versions older than 5.7.9.
# The $LOG_ALLOW_UPGRADE=1 iff an empty redo log is allowed to be upgraded.
#
# In $ZIP_FILE we have data directory with logically empty redo log,
# after clean shutdown on mysql-$TESTED_DATADIR_VERSION.
#
# The $LOG_FORMAT_MESSAGE is a message expected to be seen in error log
# when starting MySQL (unless --innodb-force-recovery > 0 or
# --innodb-read-only = ON).
#

--source include/have_innodb_16k.inc

let MYSQLD_DATADIR = $MYSQLTEST_VARDIR/tmp/log_corruption;

--echo # Create an empty data directory...
--mkdir $MYSQLD_DATADIR

let SEARCH_FILE = $MYSQLD_DATADIR/my_restart_attempt.err;

--echo # Unzip $ZIP_FILE to the created directory...
--exec unzip $MYSQL_TEST_DIR/std_data/log_corruption/$ZIP_FILE -d $MYSQLD_DATADIR > $SEARCH_FILE

--file_exists $SEARCH_FILE

let MYSQLD_ARGS = --datadir=$MYSQLD_DATADIR --console --skip-log-bin --log-error-verbosity=3 --lower_case_table_names=1 > $SEARCH_FILE 2>&1 ;

let IB_LOGFILE0_PATH = $MYSQLD_DATADIR/ib_logfile0;
let IB_LOGFILE1_PATH = $MYSQLD_DATADIR/ib_logfile1;

--file_exists $IB_LOGFILE0_PATH
--file_exists $IB_LOGFILE1_PATH

--source redo_log_error_patterns.inc

--source include/kill_mysqld.inc
