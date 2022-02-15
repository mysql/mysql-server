--move_file $MYSQLD_ERROR_LOG $MYSQLTEST_VARDIR/log/saved_my_restart.err
--force-rmdir $MYSQLD_DATADIR

let MYSQLD_ARGS=;
let MYSQLD_EXTRA_ARGS=;
let MYSQLD_DATADIR=;
let MYSQLD_ERROR_LOG=;

let $restart_parameters = restart:;
--source include/start_mysqld.inc

--source include/wait_until_count_sessions.inc
