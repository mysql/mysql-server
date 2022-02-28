--source include/have_debug.inc

--source include/big_test.inc

--source include/count_sessions.inc

let $tmp_dir = $MYSQLTEST_VARDIR/tmp;
let MYSQLD_DATADIR = $tmp_dir/redo_log_test;
let MYSQLD_ERROR_LOG = $tmp_dir/my_restart.err;
let $INNODB_PAGE_SIZE = `select @@innodb_page_size`;
let MYSQLD_EXTRA_ARGS = --innodb-redo-log-capacity=8388608 --innodb_page_size=$INNODB_PAGE_SIZE;

--echo # Initialize new data directory...
--source include/initialize_datadir.inc

let $restart_parameters = restart: --datadir=$MYSQLD_DATADIR --log-error=$MYSQLD_ERROR_LOG $MYSQLD_EXTRA_ARGS;
--replace_result $MYSQLD_ERROR_LOG ERROR_LOG $MYSQLD_DATADIR DATADIR $INNODB_PAGE_SIZE INNODB_PAGE_SIZE
--source include/restart_mysqld.inc

--echo # Prepare schema used in the tests.
--source include/ib_log_spammer_init.inc

--echo # Disable checkpointing.
SET GLOBAL innodb_checkpoint_disabled = ON;

--echo # Create connection which generates spam to the redo log.
--connect(C1,localhost,root,,test)
--send CALL log_spammer()
--connect(C2,localhost,root,,test)
--send CALL log_spammer()
--connection default

--source include/ib_redo_log_files_count_wait.inc

# Note, that in theory it is possible that the checkpoint exists in the second redo file and
# the first redo file gets consumed after observing two files are present. In such case, the
# test needs to detect that and most likely use "--skip" to skip its execution. It is rather
# unlikely scenario.

--source include/kill_mysqld.inc
