# WL#15369 Add progress information to the error log during shutdown
# We try to hang connection threads to verify if ER_THREAD_STILL_ALIVE,
# and ER_NUM_THREADS_STILL_ALIVE are being logged.

--source include/big_test.inc
--source include/have_debug.inc

--echo # connect as root from root_con1, and make the thread sleep
connect(root_con1,localhost,root,,mysql);
SET DEBUG="+d,simulate_connection_thread_hang";
--send SELECT 1

--echo # connect as root from root_con2, and make the thread sleep
connect(root_con2,localhost,root,,mysql);
SET DEBUG="+d,simulate_connection_thread_hang";
--send SELECT 1

connection default;
sleep 3;

--echo # ---------------------------------------------------
--echo # shut server down

--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down

--echo # close the test connections

disconnect root_con1;
disconnect root_con2;
connection default;

--echo # ---------------------------------------------------
--echo # Read the logs

let SEARCH_FILE= $MYSQLTEST_VARDIR/log/mysqld.1.err;

--echo # looking for ER_THREAD_STILL_ALIVE
let SEARCH_PATTERN= Waiting for forceful disconnection of Thread;
--source include/search_pattern.inc
--echo # Search completed

--echo # looking for ER_NUM_THREADS_STILL_ALIVE
let SEARCH_PATTERN= Waiting for forceful disconnection of;
--source include/search_pattern.inc
--echo # Search completed

--echo # ---------------------------------------------------
--echo # Clean Up

--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # ---------------------------------------------------
