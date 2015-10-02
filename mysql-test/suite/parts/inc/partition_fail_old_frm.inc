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

--echo # State before failure
--source include/get_frm_info.inc
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
--disable_abort_on_error
ALTER TABLE t1 UPGRADE PARTITIONING;
--enable_abort_on_error
--echo # State after failure
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

--echo # Same test under LOCK TABLE

--source include/create_575_part_table.inc

--echo # Flush the table data files to get deterministic checksum of data files
FLUSH TABLE t1 FOR EXPORT;
let LIST_OUT_FILE=$MYSQLD_DATADIR/test/list_t1_1.txt;
--source include/list_files_with_size_and_hash.inc
UNLOCK TABLES;

--echo # State before failure
--source include/get_frm_info.inc
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
LOCK TABLE t1 WRITE;
--disable_abort_on_error
ALTER TABLE t1 UPGRADE PARTITIONING;
--enable_abort_on_error
--echo # State after failure
--source include/get_frm_info.inc
--replace_result #p# #P# #sp# #SP#
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
UNLOCK TABLES;

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
