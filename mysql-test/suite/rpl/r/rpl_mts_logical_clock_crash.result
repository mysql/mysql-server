include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1(c1 INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2(c1 INT PRIMARY KEY) ENGINE = InnoDB;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
[connection master]
SET @save_debug = @@GLOBAL.debug;
SET GLOBAL debug = "d,set_commit_parent_100";
INSERT INTO t2 VALUES(99);
BEGIN;
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
COMMIT;
BEGIN;
INSERT INTO t1 VALUES(3);
INSERT INTO t1 VALUES(4);
COMMIT;
INSERT INTO t1 VALUES(5);
SET GLOBAL debug = @save_debug;
include/save_master_pos.inc
[connection slave]
SET @save_slave_parallel_type = @@GLOBAL.slave_parallel_type;
SET @save_slave_parallel_workers = @@GLOBAL.slave_parallel_workers;
SET @save_slave_transaction_retries = @@GLOBAL.slave_transaction_retries;
SET GLOBAL slave_parallel_type = LOGICAL_CLOCK;
SET GLOBAL slave_parallel_workers = 2;
SET GLOBAL slave_transaction_retries = 0;
INSERT INTO t1 VALUES(1);
CALL mtr.add_suppression("Worker . Failed executing transaction.*");
CALL mtr.add_suppression(".*coordinator and worker threads are stopped.*");
LOCK TABLE t1 WRITE;
[connection server_2]
include/sync_slave_io.inc
include/start_slave_sql.inc
[connection slave]
UNLOCK TABLES;
include/wait_for_slave_sql_to_stop.inc
SET GLOBAL slave_parallel_workers = @save_slave_parallel_workers;
SET GLOBAL slave_parallel_type = @save_slave_parallel_type;
SET GLOBAL slave_transaction_retries = @save_slave_transaction_retries;
TRUNCATE t1;
include/start_slave_sql.inc
[connection master]
DROP TABLE t1;
DROP TABLE t2;
include/rpl_end.inc
