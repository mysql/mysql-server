include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("Deadlock found");
**** On Master ****
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
INSERT INTO t1 VALUES (1,1), (2,2), (3,3), (4,4);
**** On Slave ****
include/sync_slave_sql_with_master.inc
SELECT count_transactions_retries FROM performance_schema.replication_applier_status;
count_transactions_retries
0
set @@global.slave_exec_mode= 'IDEMPOTENT';
UPDATE t1 SET a = 5, b = 47 WHERE a = 1;
SELECT * FROM t1 order by a;
a	b
2	2
3	3
4	4
5	47
**** On Master ****
UPDATE t1 SET a = 5, b = 5 WHERE a = 1;
SELECT * FROM t1 order by a;
a	b
2	2
3	3
4	4
5	5
**** On Slave ****
include/sync_slave_sql_with_master.inc
set @@global.slave_exec_mode= default;
SELECT count_transactions_retries FROM performance_schema.replication_applier_status;
count_transactions_retries
0
SELECT * FROM t1 order by a;
a	b
2	2
3	3
4	4
5	47
include/check_slave_is_running.inc
call mtr.add_suppression("Slave SQL.*Could not execute Update_rows event on table test.t1");
call mtr.add_suppression("Slave SQL for channel '': worker thread retried transaction");
call mtr.add_suppression("The slave coordinator and worker threads are stopped");
set @save_innodb_lock_wait_timeout=@@global.innodb_lock_wait_timeout;
set @save_slave_transaction_retries=@@global.slave_transaction_retries;
set @@global.innodb_lock_wait_timeout=1;
set @@global.slave_transaction_retries=2;
include/restart_slave.inc
BEGIN;
INSERT INTO t1 SET a = 6, b = 7;
INSERT INTO t1 SET a = 99, b = 99;
XA START 'xa1';
INSERT INTO t1 SET a = 6, b = 6;
XA END 'xa1';
XA PREPARE 'xa1';
include/wait_for_slave_sql_error.inc [errno=1213,1205]
set @@global.innodb_lock_wait_timeout=1;
set @@global.slave_transaction_retries=100;
SELECT count_transactions_retries FROM performance_schema.replication_applier_status INTO @last_retries;
include/start_slave_sql.inc
ROLLBACK;
XA COMMIT 'xa1';
include/sync_slave_sql_with_master.inc
include/assert.inc [XA transaction record must be in the table]
set @@global.innodb_lock_wait_timeout=@save_innodb_lock_wait_timeout;
set @@global.slave_transaction_retries= @save_slave_transaction_retries;
**** On Master ****
DROP TABLE t1;
include/rpl_end.inc
