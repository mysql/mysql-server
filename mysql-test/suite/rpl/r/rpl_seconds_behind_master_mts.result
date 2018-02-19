include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Adding debug point 'dbug.mts.force_clock_diff_eq_0' to @@GLOBAL.debug
SET @save_slave_parallel_type = @@GLOBAL.slave_parallel_type;
SET @save.slave_parallel_workers= @@global.slave_parallel_workers;
SET @save_slave_transaction_retries= @@global.slave_transaction_retries;
# Test for PARALLEL_DATABASE
SET GLOBAL slave_parallel_type = 'DATABASE';
SET @@global.slave_parallel_workers= 4;
include/start_slave.inc
# Scenario 1: With two workers, check if Seconds_Behind_Master is
# calculated correctly when all the workers are waiting.
CREATE TABLE t1 (f1 INT);
CREATE DATABASE test2;
USE test2;
CREATE TABLE t2 (f1 INT);
include/sync_slave_sql_with_master.inc
LOCK TABLE test.t1 WRITE;
LOCK TABLE test2.t2 WRITE;
USE test;
INSERT INTO t1 VALUES (1);
USE test2;
INSERT INTO t2 VALUES (1);
include/sync_slave_io_with_master.inc
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 5 and upper_bound]
UNLOCK TABLES;
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 2 and upper_bound]
UNLOCK TABLES;
include/sync_slave_sql_with_master.inc
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be 0]
# Scenario 2: With two workers, check if Seconds_Behind_Master is
# calculated correctly when first worker compeletes first.
LOCK TABLE test2.t2 WRITE;
USE test;
INSERT INTO t1 VALUES (1);
USE test2;
INSERT INTO t2 VALUES (1);
include/sync_slave_io_with_master.inc
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 2 and upper_bound]
UNLOCK TABLES;
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be 0]
# Scenario 3: With three workers, check if Seconds_Behind_Master is
# calculated correctly when second worker completes first.
CREATE DATABASE test3;
USE test3;
CREATE TABLE t3 (f1 INT);
include/sync_slave_sql_with_master.inc
LOCK TABLE test.t1 WRITE;
LOCK TABLE test3.t3 WRITE;
USE test;
INSERT INTO t1 VALUES (1);
use test2;
INSERT INTO t2 VALUES (1);
USE test3;
INSERT INTO t3 VALUES (1);
include/sync_slave_io_with_master.inc
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 7 and upper_bound]
UNLOCK TABLES;
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 2 and upper_bound]
UNLOCK TABLES;
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be 0]
DROP TABLE test.t1;
DROP DATABASE test2;
DROP DATABASE test3;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
# Test for LOGICAL_CLOCK
SET GLOBAL slave_parallel_type = 'LOGICAL_CLOCK';
SET GLOBAL slave_parallel_workers = 3;
include/start_slave.inc
# Scenario 1: With two workers, check if Seconds_Behind_Master is
# calculated correctly when two statements are applied in parallel on
# one database and both the workers are blocked to to lock.
USE test;
CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int);
INSERT INTO t1 SET a=1;
include/sync_slave_sql_with_master.inc
LOCK TABLE test.t1 WRITE, test.t2 WRITE;
BEGIN;
INSERT INTO t1 SET a=2;
BEGIN;
INSERT INTO t2 SET a=4;
COMMIT;
COMMIT;
include/sync_slave_io_with_master.inc
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 3 and upper_bound]
UNLOCK TABLES;
include/sync_slave_sql_with_master.inc
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be 0]
# Scenario 2: With two workers, check if Seconds_Behind_Master is
# calculated correctly when the first worker is blocked.
INSERT INTO t1 SET a=1;
include/sync_slave_sql_with_master.inc
LOCK TABLE test.t1 WRITE;
LOCK TABLE test.t2 WRITE;
BEGIN;
INSERT INTO t1 SET a=2;
BEGIN;
INSERT INTO t2 SET a=3;
COMMIT;
COMMIT;
include/sync_slave_io_with_master.inc
UNLOCK TABLES;
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 2 and upper_bound]
UNLOCK TABLES;
include/sync_slave_sql_with_master.inc
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be 0]
# Scenario 3: With three workers, check if Seconds_Behind_Master is
# calculated correctly when two workers working on table from the same
# database and 3rd worker working on another database.
CREATE DATABASE second_test;
CREATE TABLE second_test.t3 (f1 INT);
INSERT INTO test.t1 SET a=1;
LOCK TABLE test.t1 WRITE;
LOCK TABLE test.t2 WRITE;
LOCK TABLE second_test.t3 WRITE;
BEGIN;
INSERT INTO second_test.t3 VALUES (1);
BEGIN;
INSERT INTO test.t1 SET a=2;
BEGIN;
INSERT INTO test.t2 SET a=3;
COMMIT;
COMMIT;
COMMIT;
include/sync_slave_io_with_master.inc
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 2 and upper_bound]
UNLOCK TABLES;
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 3 and upper_bound]
UNLOCK TABLES;
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be between 4 and upper_bound]
UNLOCK TABLE;
# Adding debug point 'mts_checkpoint' to @@GLOBAL.debug
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
# Removing debug point 'mts_checkpoint' from @@GLOBAL.debug
include/assert.inc [Seconds_Behind_Master must be 0]
DROP TABLE test.t1;
DROP TABLE test.t2;
DROP DATABASE second_test;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
# Removing debug point 'dbug.mts.force_clock_diff_eq_0' from @@GLOBAL.debug
SET @@GLOBAL.slave_parallel_type= @save_slave_parallel_type;
SET @@global.slave_parallel_workers= @save.slave_parallel_workers;
SET @@global.slave_transaction_retries= @save_slave_transaction_retries;
include/start_slave.inc
include/rpl_end.inc
