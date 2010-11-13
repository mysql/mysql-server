# Include file to decrease test code duplication

--eval $create_statement
--eval $insert_statement
--echo # State before crash
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--disable_reconnect
# CR_SERVER_LOST
--error 2013
--eval $crash_statement
--echo # State after crash (before recovery)
--replace_regex /sqlx.*\./sqlx-nnnn_nnnn./ /#p#/#P#/ /#sp#/#SP#/ /#tmp#/#TMP#/
--list_files $DATADIR/test
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--source include/wait_until_connected_again.inc
--echo # State after crash recovery
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;
