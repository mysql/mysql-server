include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
*** Prepare tables and data ***
CREATE TABLE t1 (a INT NOT NULL, KEY(a)) ENGINE=innodb;
CREATE TABLE t2 (a INT) ENGINE=innodb;
CREATE TABLE t3 (a INT NOT NULL, KEY(a)) ENGINE=innodb;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  KEY `a` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `a` int NOT NULL,
  KEY `a` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT @@GLOBAL.replica_transaction_retries;
@@GLOBAL.replica_transaction_retries
2
SELECT @@GLOBAL.innodb_lock_wait_timeout;
@@GLOBAL.innodb_lock_wait_timeout
4
include/stop_slave.inc
BEGIN;
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (2), (2), (2), (2), (2), (2), (2), (2), (2), (2);
INSERT INTO t3 VALUES (3);
COMMIT;

*** Test deadlock ***
BEGIN;
SELECT * FROM t1 FOR UPDATE;
a
include/start_slave.inc
SELECT COUNT(*) FROM t2;
COUNT(*)
0
COMMIT;

# Test that the performance schema column shows > 0 values.

include/assert.inc [current number of retries should be more than the value saved before deadlock.]
include/check_slave_is_running.inc
SELECT * FROM t1;
a
1
SELECT * FROM t3;
a
3

*** Test lock wait timeout ***
include/stop_slave.inc
DELETE FROM t2;
RESET MASTER;
CHANGE REPLICATION SOURCE TO SOURCE_LOG_POS=MASTER_POS_BEGIN;
BEGIN;
SELECT * FROM t1 FOR UPDATE;
a
1
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_error.inc [errno=1205]
SELECT COUNT(*) FROM t2;
COUNT(*)
0
COMMIT;
include/start_slave.inc
SELECT * FROM t1;
a
1
1
SELECT * FROM t3;
a
3
3
include/check_slave_is_running.inc

*** Test lock wait timeout and purged relay logs ***
SET @my_max_relay_log_size= @@global.max_relay_log_size;
SET global max_relay_log_size=0;
include/stop_slave.inc
DELETE FROM t2;
RESET MASTER;
CHANGE REPLICATION SOURCE TO SOURCE_LOG_POS=MASTER_POS_BEGIN;
BEGIN;
SELECT * FROM t1 FOR UPDATE;
a
1
1
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_error.inc [errno=1205]
SELECT COUNT(*) FROM t2;
COUNT(*)
0
COMMIT;
include/start_slave.inc
SELECT * FROM t1;
a
1
1
1
SELECT * FROM t3;
a
3
3
3
include/check_slave_is_running.inc

*** Test the deadlock warning to be escalated into the error ***
delete from t1;
delete from t2;
delete from t3;
delete from t1;
delete from t2;
delete from t3;
set @save.replica_transaction_retries= @@global.replica_transaction_retries;
set @@global.replica_transaction_retries= 0;
include/stop_slave.inc
BEGIN;
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (2), (2), (2), (2), (2), (2), (2), (2), (2), (2);
INSERT INTO t3 VALUES (3);
COMMIT;
BEGIN;
SELECT count(*) as zero FROM t1 FOR UPDATE;
zero
0
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
*** Now the slave must be stopped due to timeout ***
include/wait_for_slave_sql_error.inc [errno=1205]
rollback;
set @@global.replica_transaction_retries= @save.replica_transaction_retries;
include/start_slave.inc
*** Clean up ***
DROP TABLE t1,t2,t3;
SET global max_relay_log_size= @my_max_relay_log_size;
CALL mtr.add_suppression(".*worker thread retried transaction.*");
CALL mtr.add_suppression(".*The slave coordinator and worker threads are stopped.*");
End of 5.1 tests
include/rpl_end.inc
