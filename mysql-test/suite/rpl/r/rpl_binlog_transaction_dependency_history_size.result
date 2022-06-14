include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET @binlog_transaction_dependency_tracking_save = @@GLOBAL.binlog_transaction_dependency_tracking;

SET GLOBAL binlog_transaction_dependency_tracking = WRITESET;
######## 1. WRITESET ########
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET	3
CREATE TABLE test.tab1 (a INT PRIMARY KEY AUTO_INCREMENT NOT NULL);
CREATE TABLE test.tab2 (a INT);
FLUSH LOGS;
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab2 VALUES (1);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL), (NULL), (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
BEGIN;
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
COMMIT;
# Drop table and flush logs to force binlog to rotate
DROP TABLE test.tab1, test.tab2;
FLUSH LOGS;
# Processing binlog...
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;3 4;1 5;5 6;5 7;7 8;7 9;7 10;7 11;11 12;9 13;13 14]
# Verify that replication is correct
include/sync_slave_sql_with_master.inc

SET GLOBAL binlog_transaction_dependency_tracking = WRITESET_SESSION;
######## 2. WRITESET_SESSION ########
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET_SESSION	3
CREATE TABLE test.tab1 (a INT PRIMARY KEY AUTO_INCREMENT NOT NULL);
CREATE TABLE test.tab2 (a INT);
FLUSH LOGS;
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab2 VALUES (1);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL), (NULL), (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
BEGIN;
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
COMMIT;
# Drop table and flush logs to force binlog to rotate
DROP TABLE test.tab1, test.tab2;
FLUSH LOGS;
# Processing binlog...
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;3 4;1 5;5 6;5 7;7 8;7 9;7 10;7 11;11 12;9 13;13 14]
# Verify that replication is correct
include/sync_slave_sql_with_master.inc

SET GLOBAL binlog_transaction_dependency_tracking = COMMIT_ORDER;
######## 3. COMMIT_ORDER ########
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
COMMIT_ORDER	3
CREATE TABLE test.tab1 (a INT PRIMARY KEY AUTO_INCREMENT NOT NULL);
CREATE TABLE test.tab2 (a INT);
FLUSH LOGS;
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab2 VALUES (1);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL), (NULL), (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
BEGIN;
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
INSERT INTO tab1 VALUES (NULL);
COMMIT;
# Drop table and flush logs to force binlog to rotate
DROP TABLE test.tab1, test.tab2;
FLUSH LOGS;
# Processing binlog...
include/include/assert_logical_timestamps.inc [0 1;1 2;2 3;3 4;4 5;5 6;6 7;7 8;8 9;9 10;10 11;11 12;9 13;13 14]
# Verify that replication is correct
include/sync_slave_sql_with_master.inc
SET GLOBAL binlog_transaction_dependency_tracking = @binlog_transaction_dependency_tracking_save;
include/rpl_end.inc
