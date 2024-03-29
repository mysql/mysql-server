--move_file $MYSQLD_ERROR_LOG $MYSQLTEST_VARDIR/log/saved_my_restart.err
--force-rmdir $MYSQLD_DATADIR

let MYSQLD_ARGS=;
let MYSQLD_EXTRA_ARGS=;
let MYSQLD_DATADIR=;
let MYSQLD_ERROR_LOG=;

let $restart_parameters = restart: --innodb_page_size=$INNODB_PAGE_SIZE;
--replace_result $INNODB_PAGE_SIZE INNODB_PAGE_SIZE
--source include/start_mysqld.inc

--source include/wait_until_count_sessions.inc
