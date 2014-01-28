include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CALL mtr.add_suppression('Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT');
CREATE TABLE t1 (a VARCHAR(1000));
INSERT INTO t1 VALUES (CONNECTION_ID());
INSERT INTO t1 VALUES (CONNECTION_ID());
INSERT INTO t1 VALUES
(CURDATE()),
(CURRENT_DATE()),
(CURRENT_TIME()),
(CURRENT_TIMESTAMP()),
(CURTIME()),
(LOCALTIME()),
(LOCALTIMESTAMP()),
(NOW()),
(UNIX_TIMESTAMP()),
(UTC_DATE()),
(UTC_TIME()),
(UTC_TIMESTAMP());
INSERT INTO t1 VALUES (RAND());
INSERT INTO t1 VALUES (LAST_INSERT_ID());
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1, slave:t1]
DROP TABLE t1;
include/rpl_end.inc
