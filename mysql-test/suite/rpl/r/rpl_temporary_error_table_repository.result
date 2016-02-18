include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection master]
CREATE TABLE t1 (c1 INT PRIMARY KEY, c2 INT) ENGINE= InnoDB;
include/stop_slave.inc
SET @saved_slave_transaction_retries = @@GLOBAL.slave_transaction_retries;
SET @saved_innodb_lock_wait_timeout = @@GLOBAL.innodb_lock_wait_timeout;
SET @saved_master_info_repository = @@GLOBAL.master_info_repository;
SET @saved_relay_log_info_repository = @@GLOBAL.relay_log_info_repository;
SET @@GLOBAL.innodb_lock_wait_timeout = 1;
SET @@GLOBAL.master_info_repository = TYPE;
SET @@GLOBAL.relay_log_info_repository = TYPE;
[connection slave1]
BEGIN;
INSERT INTO t1 VALUES (7,0);
[connection master]
INSERT INTO t1 VALUES (7,7);
[connection slave]
SET @@GLOBAL.slave_transaction_retries = 0;
include/start_slave.inc
### Specified retry number is expected ###
### Timeout error is expected ###
include/wait_for_slave_sql_error.inc [errno=1205]
[connection slave]
SET @@GLOBAL.slave_transaction_retries = 1;
include/start_slave.inc
### Specified retry number is expected ###
### Timeout error is expected ###
include/wait_for_slave_sql_error.inc [errno=1205]
[connection slave1]
ROLLBACK;
#### Cleanup ####
[connection slave]
include/stop_slave.inc
SET @@GLOBAL.slave_transaction_retries = @saved_slave_transaction_retries;
SET @@GLOBAL.master_info_repository = @saved_master_info_repository;
SET @@GLOBAL.relay_log_info_repository = @saved_relay_log_info_repository;
SET @@GLOBAL.innodb_lock_wait_timeout = @saved_innodb_lock_wait_timeout ;
include/start_slave.inc
[connection master]
DROP TABLE t1;
include/rpl_end.inc
