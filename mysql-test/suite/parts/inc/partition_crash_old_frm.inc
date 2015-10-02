# Include file to decrease test code duplication

let $MYSQLD_DATADIR=`SELECT @@datadir`;
let PATH_TO_TABLE=$MYSQLD_DATADIR/test/t1;
let FILE_LIST_PATTERN=$MYSQLD_DATADIR/test/t1*ibd;
let $TABLE_NAME=t1;

--source include/create_575_part_table.inc

--echo # Flush the table data files to get deterministic checksum of data files
FLUSH TABLE t1 FOR EXPORT;
let LIST_OUT_FILE=$MYSQLD_DATADIR/test/list_t1_1.txt;
--source include/list_files_with_size_and_hash.inc
UNLOCK TABLES;

--echo # State before crash
--source include/get_frm_info.inc
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--disable_reconnect
# CR_SERVER_LOST
--error 2013
ALTER TABLE t1 UPGRADE PARTITIONING;
--echo # State after crash (before recovery)
--replace_regex /sqlx.*\./sqlx-nnnn_nnnn./ /#p#/#P#/ /#sp#/#SP#/ /#tmp#/#TMP#/
--list_files $DATADIR/test
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
let $WAIT_COUNT=6000;
--source include/wait_time_until_connected_again.inc
--echo # State after crash recovery
--source include/get_frm_info.inc
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;

--echo # Flush the table data files to get deterministic checksum of data files
FLUSH TABLE t1 FOR EXPORT;
let LIST_OUT_FILE=$MYSQLD_DATADIR/test/list_t1_2.txt;
--source include/list_files_with_size_and_hash.inc
UNLOCK TABLES;
--echo # Verifying that the data files is the same
--diff_files $MYSQLD_DATADIR/test/list_t1_1.txt $MYSQLD_DATADIR/test/list_t1_2.txt
--remove_file $MYSQLD_DATADIR/test/list_t1_1.txt
--remove_file $MYSQLD_DATADIR/test/list_t1_2.txt

DROP TABLE t1;
