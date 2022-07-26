################################################################################
# It verifys that transaction retry works well on MTS. It is designed to work
# on both database and logical clock MTS.
#
# Test Scenarios:
# 1. replica_transaction_retries = 0, coordinator and workers should stop without
#    retry when encountering a temporary error.
# 2. coordinator and workers should stop after retrying a transaction
#    'replica_transaction_retries' times.
# 3. the worker can continue to finish the transaction correctly if retry does't
#    encounter any error and succeeds.
#
# Test Logic:
# 1. Inserts some data into tables on master and sync the data to slave.
# 2. Locks all rows on slave through SELECT ... FOR UPDATE
# 3. Inserts some new data into tables on master.
#    Slave workers will encounter the temporary error 'Lock wait timeout
#    exceeded' when applying the events.
# 4. check some status to make sure it acts as expected.
#
# In the test cases, we setup two slave workers. So it initializes two databases.
# in each test case, it will use two transactions. They operate on different
# databases. For databases MTS, it works. For logic clock MTS, the caller should
# do --let $set_commit_parent_100=1 before this include file. So the two
# transactions can be paralleled on slave.
################################################################################
--echo #
--echo # Initialization the test
--echo #

# Create two database to support database MTS.
# For logical clock MTS the below DDLs and inserts should
# be run on slave in their original order.
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t1(c1 INT PRIMARY KEY, c2 VARCHAR(4096)) ENGINE=InnoDB;
CREATE TABLE db1.t2(c1 INT PRIMARY KEY, c2 VARCHAR(4096)) ENGINE=InnoDB;
CREATE TABLE db2.t1(c1 INT PRIMARY KEY, c2 VARCHAR(4096)) ENGINE=InnoDB;

INSERT INTO db1.t1 VALUES(1, NULL), (10, NULL), (100, NULL);
INSERT INTO db1.t2 VALUES(1, NULL), (10, NULL), (100, NULL);
INSERT INTO db2.t1 VALUES(1, NULL), (10, NULL), (100, NULL);

--source include/sync_slave_sql_with_master.inc
CALL mtr.add_suppression("Slave SQL for channel '':.*Lock wait timeout exceeded.*");
CALL mtr.add_suppression("Slave SQL for channel '': Worker . failed executing transaction.*");
CALL mtr.add_suppression("Slave SQL for channel '': .*Error_code: MY-001756");
CALL mtr.add_suppression("Slave SQL for channel '': worker thread retried transaction.*");

# Backup the original value of the variables
SET @saved_innodb_lock_wait_timeout = @@GLOBAL.innodb_lock_wait_timeout;
SET @saved_replica_parallel_workers = @@GLOBAL.replica_parallel_workers;
SET @saved_replica_parallel_type = @@GLOBAL.replica_parallel_type;
SET @saved_max_relay_log_size = @@GLOBAL.max_relay_log_size;
SET @saved_replica_transaction_retries = @@GLOBAL.replica_transaction_retries;

# Set a small wait timeout, so the test can run fast
SET GLOBAL innodb_lock_wait_timeout = 1;
SET GLOBAL replica_parallel_workers = 2;

# Set a small relay log size, so the events of a transaction will be divided
# into more than one relay logs.
SET GLOBAL max_relay_log_size = 4096;

--source include/stop_slave_sql.inc

eval SET GLOBAL replica_parallel_type = $mts_parallel_type;

--source include/start_slave_sql.inc

--echo #
--echo # Case 1: slave will stop if replica_transaction_retries is 0
--echo #
SET GLOBAL replica_transaction_retries = 0;

# Lock all rows of db1.t1
BEGIN;
SELECT c1 FROM db1.t1 FOR UPDATE;

--source include/rpl_connection_master.inc
if ($set_commit_parent_100)
{
# Let the two inserts and later transactions run in parallel on slave.
  SET DEBUG = "+d,set_commit_parent_100";
}
INSERT INTO db1.t1 VALUES(2, NULL);
INSERT INTO db2.t1 VALUES(2, NULL);
--source include/save_master_pos.inc

--source include/rpl_connection_slave.inc
# Lock wait timeout
--let $slave_sql_errno= convert_error(ER_LOCK_WAIT_TIMEOUT)
--source include/wait_for_slave_sql_error.inc

--let $assert_text= Value 2 is not in db1.t1.
--let $assert_cond= count(*) = 0 FROM db1.t1 WHERE c1 = 2
--source include/assert.inc

# release the lock on db1.t1
ROLLBACK;

--echo #
--echo # Case 2: Slave will stop after retrying a transaction
--echo #         replica_transaction_retries times.
--echo #
SET GLOBAL replica_transaction_retries = 2;
--source include/start_slave_sql.inc
--source include/sync_slave_sql.inc

# Lock all rows of db1.t2
BEGIN;
SELECT c1 FROM db1.t2 FOR UPDATE;


--source include/rpl_connection_master.inc
BEGIN;
INSERT INTO db1.t1 VALUES(20, NULL);
INSERT INTO db1.t2 VALUES(20, NULL);
COMMIT;

INSERT INTO db2.t1 VALUES(20, NULL);
--source include/save_master_pos.inc

--source include/rpl_connection_slave.inc
# Lock wait timeout
--let $slave_sql_errno= convert_error(ER_LOCK_WAIT_TIMEOUT)
--source include/wait_for_slave_sql_error.inc

--let $assert_text= Value 20 is not in db1.t1.
--let $assert_cond= count(*) = 0 FROM db1.t1 WHERE c1 = 20
--source include/assert.inc

# release the lock on db1.t2
ROLLBACK;

--echo #
--echo # Case 3: Slave worker will not stop after retring a transaction
--echo #         sucessfully.
--echo #
--let $retried_trans= query_get_value(select count_transactions_retries from performance_schema.replication_applier_status, count_transactions_retries, 1)
SET GLOBAL replica_transaction_retries = 10;
--source include/start_slave_sql.inc
--source include/sync_slave_sql.inc
--source include/check_slave_no_error.inc

# Lock all rows of db1.t2
BEGIN;
SELECT c1 FROM db1.t2 FOR UPDATE;

--source include/rpl_connection_master.inc
--let $str= `SELECT repeat('a', 4096)`
BEGIN;
# It will make below INSERTs to be stored into different relay logs.
--echo # INSERT INTO db1.t1 VALUES(30, 4096'a')
--echo # INSERT INTO db1.t1 VALUES(31, 4096'a')
--disable_query_log
eval INSERT INTO db1.t1 VALUES(30, '$str');
eval INSERT INTO db1.t1 VALUES(31, '$str');
--enable_query_log

# To Verify that Uservar_log_event works well
SET @str= 'abc';
eval INSERT INTO db1.t2 VALUES(30, @str);
COMMIT;

INSERT INTO db2.t1 VALUES(30, NULL);
--source include/save_master_pos.inc

--source include/rpl_connection_slave.inc
--let $status_col_comparsion= >=
--let $status_col= count_transactions_retries
--let $table=replication_applier_status
--let $status_col_value= $retried_trans+2
--let $status_fail_query= SELECT GLOBAL.slave_retried_transactions
--source include/wait_for_rpl_pfs_status.inc

#
# It is for verifying the fix of Bug#19282301
# To verify the temporary error is not reported through SHOW SLAVE STATUS
--source include/check_slave_no_error.inc

# Release the lock of db1.t2
ROLLBACK;

--source include/sync_slave_sql.inc

# It is for verifying the fix of Bug#19282301
--source include/check_slave_no_error.inc

--let $rpl_diff_statement= SELECT * FROM db1.t1
--source include/rpl_diff.inc

--let $rpl_diff_statement= SELECT * FROM db1.t2
--source include/rpl_diff.inc

--let $rpl_diff_statement= SELECT * FROM db2.t1
--source include/rpl_diff.inc

--source include/rpl_connection_master.inc
INSERT INTO db1.t1 VALUES(40, NULL);
INSERT INTO db2.t1 VALUES(40, NULL);
--source include/sync_slave_sql_with_master.inc

--echo #
--echo # Case 4: Non-temporary error will stop slave workers without retrying
--echo #
--let $retried_trans= query_get_value(select count_transactions_retries from performance_schema.replication_applier_status, count_transactions_retries, 1)
INSERT INTO db1.t1 VALUES(50, NULL);

--source include/rpl_connection_master.inc
INSERT INTO db1.t1 VALUES(50, NULL);
--source include/save_master_pos.inc

--source include/rpl_connection_slave.inc
--let $slave_sql_errno= convert_error(ER_DUP_ENTRY)
--source include/wait_for_slave_sql_error.inc

--let $assert_text= count_transactions_retries should not increase
--let $assert_status_name= count_transactions_retries
--let $assert_status_value= $retried_trans
--source include/rpl_pfs_assert_status.inc

DELETE FROM db1.t1 WHERE c1 = 50;

--source include/start_slave_sql.inc
--source include/sync_slave_sql.inc

--echo #
--echo # Cleanup
--echo #
SET GLOBAL innodb_lock_wait_timeout = @saved_innodb_lock_wait_timeout;
--disable_warnings
SET GLOBAL replica_parallel_workers = @saved_replica_parallel_workers;
--enable_warnings
SET GLOBAL max_relay_log_size = @saved_max_relay_log_size;
SET GLOBAL replica_transaction_retries = @saved_replica_transaction_retries;

--source include/stop_slave_sql.inc
SET GLOBAL replica_parallel_type= @saved_replica_parallel_type;
--source include/start_slave_sql.inc

--source include/rpl_connection_master.inc
if ($set_commit_parent_100)
{
  SET DEBUG = "-d,set_commit_parent_100";
}
DROP DATABASE db1;
DROP DATABASE db2;
