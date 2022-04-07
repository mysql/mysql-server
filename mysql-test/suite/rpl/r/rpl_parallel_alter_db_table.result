include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE DATABASE d1 CHARACTER SET UTF8MB4;
CREATE TABLE d1.t1 (a INT NOT NULL, b VARCHAR(10));
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @save_slave_parallel_type = @@GLOBAL.slave_parallel_type;
SET @save_slave_parallel_workers = @@GLOBAL.slave_parallel_workers;
SET @saved_slave_preserve_commit_order = @@GLOBAL.slave_preserve_commit_order;
SET @saved_debug= @@GLOBAL.debug;
SET GLOBAL slave_parallel_type = LOGICAL_CLOCK;
SET GLOBAL slave_parallel_workers = 4;
SET GLOBAL slave_preserve_commit_order= ON;
set GLOBAL debug="+d,delay_alter_table_by_one_second";
INSERT INTO d1.t1 VALUES(1, "");
ALTER TABLE d1.t1 ADD PRIMARY KEY (a);
ALTER DATABASE d1 DEFAULT CHARACTER SET = 'latin1';
FLUSH TABLES;
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/include/assert_logical_timestamps.inc [3 4]
include/stop_slave.inc
include/assert.inc ['ALTER DATABASE runs successfully.']
include/assert.inc ['ALTER TABLE runs successfully.']
DROP TABLE d1.t1;
DROP DATABASE d1;
SET @@GLOBAL.slave_parallel_type= @save_slave_parallel_type;
SET @@GLOBAL.slave_parallel_workers= @save_slave_parallel_workers;
SET @@GLOBAL.slave_preserve_commit_order= @saved_slave_preserve_commit_order;
SET @@GLOBAL.debug= @saved_debug;
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
