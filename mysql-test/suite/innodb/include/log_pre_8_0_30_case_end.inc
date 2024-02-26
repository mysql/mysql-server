--echo # Cleanup...
--force-rmdir $MYSQLD_DATADIR
let MYSQLD_DATADIR=;
let MYSQLD_ARGS=;
let SEARCH_FILE=;
let SEARCH_PATTERN=;
let IB_LOGFILE0_PATH=;
let IB_LOGFILE1_PATH=;

--source include/start_mysqld.inc
