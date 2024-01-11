# Include file to decrease test code duplication

--eval $create_statement
--eval $insert_statement
--echo # State before crash
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE `t1/a\b#2`;
--sorted_result
SELECT * FROM `t1/a\b#2`;
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
# CR_SERVER_LOST
--error 2013
--eval $crash_statement
--echo # State after crash (before recovery)
--replace_regex /sql.*\./sql-tmp./ /#p#/#P#/ /#sp#/#SP#/ /#tmp#/#TMP#/
--list_files $DATADIR/test
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
let $WAIT_COUNT=6000;
--source include/wait_time_until_connected_again.inc
--echo # State after crash recovery
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE `t1/a\b#2`;
--sorted_result
SELECT * FROM `t1/a\b#2`;
DROP TABLE `t1/a\b#2`;
