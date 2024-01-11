#
# Specific tests for case-insensitive file systems
# i.e. lower_case_filesystem=ON
#
-- source include/have_case_insensitive_file_system.inc

--echo #
--echo # Bug#20198490 : LOWER_CASE_TABLE_NAMES=0 ON WINDOWS LEADS TO PROBLEMS
--echo #

let SEARCH_FILE= $MYSQLTEST_VARDIR/log/my_restart.err;

--error 0,1
--remove_file $SEARCH_FILE

#Shutdown the server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc

#Start the server with --lower_case_table_names=0 in Windows.
--error 1
--exec $MYSQLD_CMD --console --lower_case_table_names=0 > $SEARCH_FILE  2>&1

#Search for the error messege in the server error log.
let SEARCH_PATTERN= \[ERROR\] \[[^]]*\] \[[^]]*\] The server option \'lower_case_table_names\' is configured to use case sensitive table names but the data directory is on a case-insensitive file system which is an unsupported combination\. Please consider either using a case sensitive file system for your data directory or switching to a case-insensitive table name mode\.;
--source include/search_pattern.inc

#Restart the server
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

#Cleanup
--error 0,1
--remove_file $SEARCH_FILE
