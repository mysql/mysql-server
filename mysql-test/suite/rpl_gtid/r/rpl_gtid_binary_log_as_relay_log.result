include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/stop_slave_sql.inc
CREATE TABLE t1(i INT);
INSERT INTO t1 values (1);
INSERT INTO t1 values (2);
include/sync_slave_io_with_master.inc
SET @save.slave_parallel_workers=@@global.slave_parallel_workers;
SET @@global.slave_parallel_workers=1;
SET @save.slave_transaction_retries=@@global.slave_transaction_retries;
SET @@global.slave_transaction_retries=0;
include/start_slave_sql.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1,slave:t1]
DROP TABLE t1;
include/stop_slave_sql.inc
SET @@global.slave_parallel_workers=@save.slave_parallel_workers;
SET @@global.slave_transaction_retries=@save.slave_transaction_retries;
include/start_slave_sql.inc
include/rpl_end.inc
