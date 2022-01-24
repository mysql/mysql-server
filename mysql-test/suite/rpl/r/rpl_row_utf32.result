include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET SQL_LOG_BIN=0;
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (c1 char(255) DEFAULT NULL, KEY c1 (c1))
ROW_FORMAT=DYNAMIC DEFAULT CHARSET=utf32;
SET sql_mode = default;
SET SQL_LOG_BIN=1;
SET @saved_replica_type_conversions= @@global.replica_type_conversions;
include/stop_slave.inc
SET GLOBAL REPLICA_TYPE_CONVERSIONS='ALL_NON_LOSSY';
include/start_slave.inc
SET SQL_LOG_BIN=0;
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 ( c1 varchar(255) DEFAULT NULL, KEY c1 (c1))
ROW_FORMAT=DYNAMIC DEFAULT CHARSET=utf32;
SET sql_mode = default;
SET SQL_LOG_BIN=1;
INSERT INTO t1(c1) VALUES ('insert into t1');
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
SET GLOBAL REPLICA_TYPE_CONVERSIONS= @saved_replica_type_conversions;
include/stop_slave.inc
include/start_slave.inc
include/rpl_end.inc
