CALL mtr.add_suppression("The option expire_logs_days cannot be used together*");
####
#### 1. When binlog_expire_logs_seconds == 0 and expire_logs_days == 0
####    no purge should happen
SET GLOBAL binlog_expire_logs_seconds= 0;
SET GLOBAL expire_logs_days= 0;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
CREATE TABLE t1(s LONGBLOB );
Case:0
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 1. FLUSH LOGS
FLUSH LOGS;
RESET MASTER;
Case:1
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 2. Binlog_size > max_binlog_size
SET @@GLOBAL.MAX_BINLOG_SIZE= 4096;
INSERT INTO t1 (s) VALUES (REPEAT('s',50000));
RESET MASTER;
Case:2
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 3. Server restart
# restart:--binlog_expire_logs_seconds=0
RESET MASTER;
##### Cleanup #####
SET @@GLOBAL.MAX_BINLOG_SIZE= 1073741824;;
DROP TABLE t1;
RESET MASTER;
####
#### 2.1: binlog_expire_logs_seconds > 0 and expire_logs_days == 0
####
Testing with smaller values of binlog_expire_logs_seconds
SET GLOBAL binlog_expire_logs_seconds= 30;
SET GLOBAL expire_logs_days= 0;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
FLUSH LOGS;
FLUSH LOGS;
FLUSH LOGS;
RESET MASTER;
Testing with greater values of binlog_expire_logs_seconds
SET GLOBAL binlog_expire_logs_seconds= 3600;
SET GLOBAL expire_logs_days= 0;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
CREATE TABLE t1(s LONGBLOB );
Case:0
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 1. FLUSH LOGS
FLUSH LOGS;
RESET MASTER;
Case:1
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 2. Binlog_size > max_binlog_size
SET @@GLOBAL.MAX_BINLOG_SIZE= 4096;
INSERT INTO t1 (s) VALUES (REPEAT('s',50000));
RESET MASTER;
Case:2
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 3. Server restart
# restart:--binlog_expire_logs_seconds=3600
RESET MASTER;
##### Cleanup #####
SET @@GLOBAL.MAX_BINLOG_SIZE= 1073741824;;
DROP TABLE t1;
RESET MASTER;
####
#### 2.3: binlog_expire_logs_seconds == 0 and expire_logs_days > 0
####
SET GLOBAL binlog_expire_logs_seconds= 0;
SET GLOBAL expire_logs_days= 1;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
CREATE TABLE t1(s LONGBLOB );
Case:0
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 1. FLUSH LOGS
FLUSH LOGS;
RESET MASTER;
Case:1
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 2. Binlog_size > max_binlog_size
SET @@GLOBAL.MAX_BINLOG_SIZE= 4096;
INSERT INTO t1 (s) VALUES (REPEAT('s',50000));
RESET MASTER;
Case:2
FLUSH LOGS;
INSERT INTO t1 VALUES('a');
FLUSH LOGS;
#### 3. Server restart
# restart:--binlog_expire_logs_seconds=86400
RESET MASTER;
##### Cleanup #####
SET @@GLOBAL.MAX_BINLOG_SIZE= 1073741824;;
DROP TABLE t1;
RESET MASTER;
SET GLOBAL binlog_expire_logs_seconds= 0;
SET GLOBAL expire_logs_days= 0;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
