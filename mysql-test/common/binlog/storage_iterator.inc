# ==== Implementation ====
#
# For all scenarios below we make use of a custom performance schema
# table that does not exist on a MySQL binary release. This table
# only exists for the purposes of running this test. Its goal is to
# have a good and clear way to show the output of the iterator and
# therefore to allow asserting and testing in an easier way.
#
# In addition to the performance schema we also introduce custom
# status variables that help asserting behavior.
#
# Scenario1: several transactions, multiple log files
#
#   Requirement:
#
#   Iterator must read log content across multiple files. 
#
#   Description:
#
#   It creates a few transactions and then asserts that the iterator
#   service is able to read those. This tests also with multiple log
#   files, by issuing FLUSH LOGS. The output of the iterator is presented
#   in a performance schema table that is only used in this test.
#
# Scenario 2: several transactions commit after the iterator is opened
#
#   Requirement:
#
#   Iterator must read log content that was produced after it was opened.
#
#   Description:
#
#   It creates a few transactions after the iterator is opened. The
#   session opening the iterator remains waiting for a signal from
#   a second session after opening the iterator. The second session
#   creates a few more transactions and then signals the first
#   session to proceed. We assert that the iterator is able to read
#   those transactions created after it was initialized.
#
# Scenario 3: several transactions commit and flush logs after the
#             iterator is opened
#
#   Requirement:
#
#   Iterator must read log content that was produced after it was
#   opened and across multiple new files.
#
#   Description:
#
#   Similar to scenario 2 but this time we introduce file rotations.
#   We assert that the iterator is able to read new transactions in
#   new log files created after the iterator had been opened.
# 
# Scenario 4: test PURGE stops while iterator is reading from a file
#             meant to be purged.
#
#   Requirement:
#
#   Iterator pinning a log file must not have its log files removed
#   by PURGE while it is using them.
#
#   Description:
#
#   Session#1 creates a few log files, with transactions in them, and
#   then opens an iterator. Session#1 now blocks waiting for a signal
#   to proceed. On Session#2 we issue PURGE LOGS TO the most recent
#   log file and then signal Session#1 to proceed. This asserts that
#   the files that Session#1 needs to read are not purged while the
#   iterator needs them.
#
# Scenario 5: issue a large transaction and then assert that the reader
#             buffer allocation count and buffer size is as expected.
#
#   Requirement:
#
#   Assert that the reader is told to allocate a larger buffer when
#   the buffer passed to the iterator is insufficient.
#
#   Description:
#
#   Session#1 create a large transaction (64 MB) and then assert that
#   the number of buffer reallocations and the size of the buffer
#   matches the expected values.
#
# Scenario 6: several transactions, multiple log files, iterator filters a few
#
#   Requirement:
#
#   The iterator skips transactions that the reader is not interested in.
#
#   Description:
#
#   Session#1 creates a few transactions scattered across multiple log
#   files. Session#2 queries the table, but activates a dbug flag to
#   skip a few. Assert that only the required ones show up in the PFS
#   table.
#
# Scenario 7: multiple log files, some purged, error when reader asks for all
#
#   Requirement:
#
#   The iterator will return an error stating that there are transactions
#   requested that do not exist in the log files.
#
#   Description:
#
#   Session#1 creates a few transactions scattered across multiple log
#   files. Purges two logs files. Session #2 asks for all transactions.
#   Error.
#
# Scenario 8: Iterator returns EOF, new transaction, caller gets extra transactions
#
#   Requirement:
#
#   The iterator will return EOF to the caller. Caller can keep the iterator
#   and if more transactions are added, he can call get again and still get
#   them.
#
#   Description:
#
#   Session#1 creates a few transactions. Then selects from the table. Hits EOF.
#   and notifies session #2 that it can proceed. Session #1 waits for session #2
#   to create new content. Session #2 creates new content and notifies Session #1.
#   Session #1, retries reading from the iterator and gets new content.
#
# Scenario 9: Restart the server so that Stop events are dealt with
#
#   Requirement:
#
#   The iterator will be able to figure the next file to iterate from even
#   if the current binary log file ends with a STOP event.
#
#   Description:
#
#   Session#1 keeps restarting the server so that STOP events are written into
#   the binary log. Then selects from the table to show that the right entries
#   are retrieved, even those that are recorded on binary log files that are
#   rotated due to the stopping of the server (and therefore have a stop event).
#
# Scenario 10: Crash the server, watch service rotate properly to next file
#
#   Requirement:
#
#   The iterator will be able to figure the next file to iterate from even
#   if the current binary log file ends without a STOP or ROTATE event
#
#   Description:
#
#   Session#1 keeps crashes the server. Recovery will truncate the file.
#   No rotate or stop event end up being written. Iterator will still be
#   able to advance and get the missing entries.
#
# ==== References ====
#
# WL#13901: service to read change streams (binlogs) from local storage
#

# no need to run this multiple times
--source include/install_replication_observers_example.inc
CALL mtr.add_suppression(".*was not purged because it was being read by.*");

RESET BINARY LOGS AND GTIDS;

################################################

--echo ###
--echo ### Scenario 1: several transactions, multiple log files
--echo ###

USE test;
--eval SET GTID_NEXT="$uuid0:1"
CREATE TABLE t1 (c1 INT);
--eval SET GTID_NEXT="$uuid0:2"
INSERT INTO t1 VALUES (1);

--eval SET GTID_NEXT="$uuid0:3"
FLUSH LOGS;
--eval SET GTID_NEXT="$uuid0:4"
INSERT INTO t1 VALUES (1);

--eval SET GTID_NEXT="$uuid0:5"
FLUSH LOGS;
--eval SET GTID_NEXT="$uuid0:6"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

--replace_regex /"position" : \d+\s\}/"position" : POSITION \}/ /"filename" : "\.\\\\(\D+)\.(\d+)"/"filename" : "\.\/\1.\2"/
SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries;

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--echo ###
--echo ### End of Scenario #1
--echo ###

################################################

--echo ###
--echo ### Scenario 2: several transactions commit after the iterator is opened.
--echo ###

--connect(con1,127.0.0.1,root,,test,$MASTER_MYPORT,)
--connect(con2,127.0.0.1,root,,test,$MASTER_MYPORT,)

## On connection 1 ##
--connection con1

USE test;
--eval SET GTID_NEXT="$uuid0:1"
CREATE TABLE t1 (c1 INT);
SET GTID_NEXT=AUTOMATIC;

--eval SET GTID_NEXT="$uuid0:2"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

SET DEBUG_SYNC='test_binlog_storage_iterator_debug_sync_opened_iterator SIGNAL signal.proceed_creating_transactions WAIT_FOR signal.resume';
--send SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries

## On connection 2 ##
--connection con2

SET DEBUG_SYNC='now WAIT_FOR signal.proceed_creating_transactions';

--let $i=5
while($i>0) {
  --eval SET GTID_NEXT="$uuid2:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --dec $i
}

SET DEBUG_SYNC='now SIGNAL signal.resume';

## On connection 1 ##
--connection con1
--replace_regex /"position" : \d+\s\}/"position" : POSITION \}/ /"filename" : "\.\\\\(\D+)\.(\d+)"/"filename" : "\.\/\1.\2"/
--reap
--connection default

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--connection con1
--disconnect con1
--connection con2
--disconnect con2
--connection default

--echo ###
--echo ### End of Scenario #2
--echo ###

################################################

--echo ###
--echo ### Scenario #3: several transactions commit after the iterator is opened and the file is rotated.
--echo ###

--connect(con1,127.0.0.1,root,,test,$MASTER_MYPORT,)
--connect(con2,127.0.0.1,root,,test,$MASTER_MYPORT,)

## On connection 1 ##
--connection con1

USE test;
--eval SET GTID_NEXT="$uuid0:1"
CREATE TABLE t1 (c1 INT);
SET GTID_NEXT=AUTOMATIC;

--eval SET GTID_NEXT="$uuid0:2"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

SET DEBUG_SYNC='test_binlog_storage_iterator_debug_sync_opened_iterator SIGNAL signal.proceed_creating_transactions WAIT_FOR signal.resume';
--send SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries

## On connection 2 ##
--connection con2

SET DEBUG_SYNC='now WAIT_FOR signal.proceed_creating_transactions';

FLUSH BINARY LOGS;

--let $i=5
while($i>0) {
  --eval SET GTID_NEXT="$uuid3:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --dec $i
}

FLUSH BINARY LOGS;

--let $i=5
while($i>0) {
  --eval SET GTID_NEXT="$uuid4:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --dec $i
}

SET DEBUG_SYNC='now SIGNAL signal.resume';

## On connection 1 ##
--connection con1
--replace_regex /"position" : \d+\s\}/"position" : POSITION \}/ /"filename" : "\.\\\\(\D+)\.(\d+)"/"filename" : "\.\/\1.\2"/
--reap
--connection default

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--connection con1
--disconnect con1
--connection con2
--disconnect con2
--connection default

--echo ###
--echo ### End of Scenario #3
--echo ###

################################################

--echo ###
--echo ### Scenario #4: test PURGE stops while iterator is reading from a file meant to be purged.
--echo ###

--disable_query_log
--echo # query log suppressed

--connect(con1,127.0.0.1,root,,test,$MASTER_MYPORT,)
--connect(con2,127.0.0.1,root,,test,$MASTER_MYPORT,)

--let $rpl_connection_name=con1
--source include/connection.inc 

--let $binlog_file1 = query_get_value(SHOW BINARY LOG STATUS, File, 1)

USE test;
--eval SET GTID_NEXT="$uuid0:1"
CREATE TABLE t1 (c1 INT);
SET GTID_NEXT=AUTOMATIC;

FLUSH BINARY LOGS;

--let $binlog_file2 = query_get_value(SHOW BINARY LOG STATUS, File, 1)

--eval SET GTID_NEXT="$uuid0:2"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

FLUSH BINARY LOGS;

--let $binlog_file3 = query_get_value(SHOW BINARY LOG STATUS, File, 1)

# lets try to purge the two first files after we open the iterator

--eval SET GTID_NEXT="$uuid0:3"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;


--enable_query_log
SET DEBUG_SYNC='test_binlog_storage_iterator_debug_sync_opened_iterator SIGNAL signal.proceed_purge_logs WAIT_FOR signal.resume';
--send SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries

--let $rpl_connection_name=con2
--source include/connection.inc 

--let $assert_text = Assert that $binlog_file1 exists in the pfs.file_instances table BEFORE the purge command
--let $assert_cond = count(*) = 1 from performance_schema.file_instances where file_name like "%$binlog_file1%"
--source include/assert.inc

--let $assert_text = Assert that $binlog_file2 exists in the pfs.file_instances table BEFORE the purge command
--let $assert_cond = count(*) = 1 from performance_schema.file_instances where file_name like "%$binlog_file2%"
--source include/assert.inc

--let $assert_text = Assert that $binlog_file3 exists in the pfs.file_instances table BEFORE the purge command
--let $assert_cond = count(*) = 1 from performance_schema.file_instances where file_name like "%$binlog_file3%"
--source include/assert.inc

SET DEBUG_SYNC='now WAIT_FOR signal.proceed_purge_logs';
--replace_result '\.\\' './'
--eval PURGE BINARY LOGS TO '$binlog_file3'

# now check that the files were not purged

--let $assert_text = Assert that $binlog_file1 exists in the pfs.file_instances table AFTER the purge command
--let $assert_cond = count(*) = 1 from performance_schema.file_instances where file_name like "%$binlog_file1%"
--source include/assert.inc

--let $assert_text = Assert that $binlog_file2 exists in the pfs.file_instances table AFTER the purge command
--let $assert_cond = count(*) = 1 from performance_schema.file_instances where file_name like "%$binlog_file2%"
--source include/assert.inc

--let $assert_text = Assert that $binlog_file3 exists in the pfs.file_instances table AFTER the purge command
--let $assert_cond = count(*) = 1 from performance_schema.file_instances where file_name like "%$binlog_file3%"
--source include/assert.inc

SET DEBUG_SYNC='now SIGNAL signal.resume';

--let $rpl_connection_name=con1
--source include/connection.inc 

--disable_query_log
--echo # query log suppressed
--disable_result_log
--echo # result log suppressed
--reap

--let $rpl_connection_name=default
--source include/connection.inc 

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--connection con1
--disconnect con1
--connection con2
--disconnect con2
--connection default

--enable_query_log
--enable_result_log

--echo ###
--echo ### End of Scenario #4
--echo ###

################################################

if (`SELECT @@global.binlog_transaction_compression = FALSE`) {

--echo ###
--echo ### Scenario #5: issue a large transaction and then assert that the reader buffer allocation count and buffer size is as expected.
--echo ###

# This scenario does not run with compression, since compression
# introduces non-determinism w.r.t. to the actual buffer size 
# needed and therefore the allocations expected may not match
# reality. We skip the test here, since this is actually tested
# without compression anyway.

--disable_query_log
--echo # query log suppressed
--disable_result_log
--echo # result log suppressed

# resets the counters
--source include/uninstall_replication_observers_example.inc
--source include/install_replication_observers_example.inc

--let $sum_buffer_requests_before = `SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME = "test_binlog_storage_iterator.sum_buffer_size_requested"`
--let $buffer_reallocations_before = `SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME = "test_binlog_storage_iterator.count_buffer_reallocations"`

CREATE TABLE t1 (c1 LONGTEXT);

--enable_query_log
--echo # query log enabled

# 64 MB insert
INSERT INTO t1 VALUES (repeat ('a', 8388608));

--replace_regex /"position" : \d+\s\}/"position" : POSITION \}/ /"filename" : "\.\\\\(\D+)\.(\d+)"/"filename" : "\.\/\1.\2"/
SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries;

--disable_query_log
--echo # query log suppressed

--let $sum_buffer_requests_after = `SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME = "test_binlog_storage_iterator.sum_buffer_size_requested"`
--let $buffer_reallocations_after = `SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME = "test_binlog_storage_iterator.count_buffer_reallocations"`

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--let $assert_cond = ($buffer_reallocations_after - $buffer_reallocations_before) = 1
--let $assert_text = Assert that there was 1 buffer reallocation
--source include/assert.inc

--let $assert_text = Assert that the sum of allocated buffer size matches ~64MB
--let $assert_cond = ($sum_buffer_requests_after - $sum_buffer_requests_before) > 8388608
--source include/assert.inc

--enable_query_log
--echo # query log enabled
--enable_result_log
--echo # result log enabled

--echo ###
--echo ### End of Scenario #5
--echo ###

}

--echo ###
--echo ### Scenario 6: several transactions, multiple log files, iterator filters a few
--echo ###

--eval SET GTID_NEXT="$uuid1:1"
CREATE TABLE t1 (c1 INT);

USE test;
--let $i=1
while($i<11) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

FLUSH LOGS;

--let $i=11
while($i<16) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

FLUSH LOGS;

--let $i=16
while($i<18) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

FLUSH LOGS;

--let $i=18
while($i<21) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

FLUSH LOGS;

--let $i=21
while($i<31) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

--let $debug_point = test_binlog_storage_iterator_filter_gtids
--echo # gtid set excluded: '11111111-1111-1111-1111-111111111111:1,00000000-0000-0000-0000-000000000000:1-5:10-15:20-25'
--source include/add_debug_point.inc
--replace_regex /"position" : \d+\s\}/"position" : POSITION \}/ /"filename" : "\.\\\\(\D+)\.(\d+)"/"filename" : "\.\/\1.\2"/
SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries WHERE entry = "Gtid";
--source include/remove_debug_point.inc

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--echo ###
--echo ### End of Scenario #6
--echo ###

#############################################

--echo ###
--echo ### Scenario 7: several transactions, multiple log files, purge, then select from table
--echo ###

--eval SET GTID_NEXT="$uuid1:1"
CREATE TABLE t1 (c1 INT);

USE test;
--let $i=1
while($i<11) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

FLUSH LOGS;

--let $i=11
while($i<16) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

FLUSH LOGS;

--let $i=16
while($i<18) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

FLUSH LOGS;

--let $i=18
while($i<21) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

FLUSH LOGS;

--let $binlog_file = query_get_value(SHOW BINARY LOG STATUS, File, 1)

--let $i=21
while($i<31) {
  --eval SET GTID_NEXT="$uuid0:$i"
  INSERT INTO t1 VALUES (1);
  SET GTID_NEXT=AUTOMATIC;
  --inc $i
}

--eval PURGE BINARY LOGS TO '$binlog_file'

--replace_regex /"position" : \d+\s\}/"position" : POSITION \}/ /"filename" : "\.\\\\(\D+)\.(\d+)"/"filename" : "\.\/\1.\2"/
--error ER_WRONG_PERFSCHEMA_USAGE
SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries;

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--echo ###
--echo ### End of Scenario #7
--echo ###

################################################

--echo ###
--echo ### Scenario 8: Iterator returns EOF, new transaction, caller gets extra transactions
--echo ###

--connect(con1,127.0.0.1,root,,test,$MASTER_MYPORT,)
--connect(con2,127.0.0.1,root,,test,$MASTER_MYPORT,)

## On connection 1 ##
--connection con1

USE test;
--eval SET GTID_NEXT="$uuid0:1"
CREATE TABLE t1 (c1 INT);
SET GTID_NEXT=AUTOMATIC;

--eval SET GTID_NEXT="$uuid0:2"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

--let $debug_point = test_binlog_storage_iterator_try_again_on_eof
--source include/add_debug_point.inc

SET DEBUG_SYNC='test_binlog_storage_iterator_debug_sync_iterator_eof SIGNAL signal.proceed_creating_transactions WAIT_FOR signal.resume';
--send SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries

## On connection 2 ##
--connection con2

# wait until reader signals that it has reached EOF
SET DEBUG_SYNC='now WAIT_FOR signal.proceed_creating_transactions';

--eval SET GTID_NEXT="$uuid0:3"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

FLUSH LOGS;

--eval SET GTID_NEXT="$uuid0:4"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

# signal that the reader can go ahead and handle EOF
SET DEBUG_SYNC='now SIGNAL signal.resume';

## On connection 1 ##
--connection con1
--replace_regex /"position" : \d+\s\}/"position" : POSITION \}/ /"filename" : "\.\\\\(\D+)\.(\d+)"/"filename" : "\.\/\1.\2"/
--reap
--source include/remove_debug_point.inc
--connection default

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--connection con1
--disconnect con1
--connection con2
--disconnect con2
--connection default

--echo ###
--echo ### End of Scenario #8
--echo ###

################################################

--echo ###
--echo ### Scenario 9: Restart the server so that Stop events are dealt with
--echo ###

--source include/restart_mysqld_no_echo.inc
USE test;
--eval SET GTID_NEXT="$uuid0:1"
CREATE TABLE t1 (c1 INT);
SET GTID_NEXT=AUTOMATIC;

--source include/restart_mysqld_no_echo.inc
--eval SET GTID_NEXT="$uuid0:2"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

--source include/restart_mysqld_no_echo.inc
--eval SET GTID_NEXT="$uuid0:3"
INSERT INTO t1 VALUES (2);
SET GTID_NEXT=AUTOMATIC;

--replace_regex /"position" : \d+\s\}/"position" : POSITION \}/ /"filename" : "\.\\\\(\D+)\.(\d+)"/"filename" : "\.\/\1.\2"/
SELECT entry, transaction_uuid, transaction_gno, storage FROM performance_schema.binlog_storage_iterator_entries;

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

--echo ###
--echo ### End of Scenario #9
--echo ###

################################################

--echo ###
--echo ### Scenario 10: Crash the server, watch service rotate properly to next file
--echo ###

USE test;
--eval SET GTID_NEXT="$uuid0:1"
CREATE TABLE t1 (c1 INT);
SET GTID_NEXT=AUTOMATIC;

--eval SET GTID_NEXT="$uuid0:2"
INSERT INTO t1 VALUES (1);
SET GTID_NEXT=AUTOMATIC;

--let $debug_point = crash_commit_before_log
--let $statement = INSERT INTO t1 VALUES (2)
--source common/binlog/server_crash_recovery.inc

--eval SET GTID_NEXT="$uuid0:4"
INSERT INTO t1 VALUES (3);
SET GTID_NEXT=AUTOMATIC;

--let $assert_text = Asset that iterator returns $uuid0:1
--let $assert_cond = [SELECT COUNT(*) FROM performance_schema.binlog_storage_iterator_entries WHERE transaction_gno=1 AND entry = "Gtid"] = 1
--source include/assert.inc

--let $assert_text = Asset that iterator returns $uuid0:2
--let $assert_cond = [SELECT COUNT(*) FROM performance_schema.binlog_storage_iterator_entries WHERE transaction_gno=2 AND entry = "Gtid"] = 1
--source include/assert.inc

--let $assert_text = Asset that iterator does not return $uuid0:3
--let $assert_cond = [SELECT COUNT(*) FROM performance_schema.binlog_storage_iterator_entries WHERE transaction_gno=3 AND entry = "Gtid"] = 0
--source include/assert.inc

--let $assert_text = Asset that iterator returns $uuid0:4
--let $assert_cond = [SELECT COUNT(*) FROM performance_schema.binlog_storage_iterator_entries WHERE transaction_gno=4 AND entry = "Gtid"] = 1
--source include/assert.inc

--let $assert_text = Assert that there is no Stop and no Rotate event
--let $assert_cond = [SELECT COUNT(*) FROM performance_schema.binlog_storage_iterator_entries WHERE entry = "Rotate" OR entry = "Stop"] = 0
--source include/assert.inc

DROP TABLE t1;
RESET BINARY LOGS AND GTIDS;

#
# Clean up
#

--source include/uninstall_replication_observers_example.inc
