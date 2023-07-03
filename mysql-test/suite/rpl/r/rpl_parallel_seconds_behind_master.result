include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/stop_slave.inc
set @save.replica_parallel_workers= @@global.replica_parallel_workers;
set @@global.replica_parallel_workers= 4;
set @save.replica_checkpoint_period= @@global.replica_checkpoint_period;
set @@global.replica_checkpoint_period= 500;
set @save.replica_transaction_retries= @@global.replica_transaction_retries;
include/start_slave.inc
create table t1 (f1 int) engine=innodb;
create table t2 (f1 int) engine=innodb;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
begin;
insert into t1 values (sleep(3) + 1);
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
commit;
begin;
insert into t2 values (1);
commit;
include/sync_slave_io_with_master.inc
lock table t2 write;
include/start_slave_sql.inc
First transaction is done, now get Seconds_Behind_Master after it...
include/wait_for_slave_param.inc [Seconds_Behind_Master]
Seconds_Behind_Master after first transaction is as expected.
unlock tables;
include/wait_for_slave_param.inc [Seconds_Behind_Master]
drop tables t1, t2;
include/sync_slave_sql_with_master.inc
set @@global.replica_checkpoint_period= @save.replica_checkpoint_period;
set @@global.replica_parallel_workers= @save.replica_parallel_workers;
set @@global.replica_transaction_retries= @save.replica_transaction_retries;
include/rpl_end.inc
