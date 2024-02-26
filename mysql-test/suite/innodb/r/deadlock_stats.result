#
# Bug #21278148  DEADLOCKS MISSED BY INFORMATION_SCHEMA.INNODB_METRICS
#                LOCK_DEADLOCKS COUNTER
#
CREATE TABLE t(a INT PRIMARY KEY, b INT) ENGINE=InnoDB;
INSERT INTO t VALUES (2,1), (1,2);
# Scenario 1: the con1 will be chosen as victim
SELECT `count` INTO @deadlocks_before_test
FROM INFORMATION_SCHEMA.INNODB_METRICS
WHERE NAME="lock_deadlocks";
BEGIN;
SELECT b FROM t WHERE a=1 FOR UPDATE;
b
2
BEGIN;
INSERT INTO t VALUES (3,3), (4,4), (5,5);
SELECT b FROM t WHERE a=2 FOR UPDATE;
b
1
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con1_will_wait';
SELECT b FROM t WHERE a=2 FOR UPDATE;;
SET DEBUG_SYNC='now WAIT_FOR con1_will_wait';
SELECT b FROM t WHERE a=1 FOR UPDATE;
b
2
ROLLBACK;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
SELECT `count` - @deadlocks_before_test AS deadlocks_during_test
FROM INFORMATION_SCHEMA.INNODB_METRICS
WHERE NAME="lock_deadlocks";
deadlocks_during_test
1
# Scenario 2: the con2 will be chosen as victim
SELECT `count` INTO @deadlocks_before_test
FROM INFORMATION_SCHEMA.INNODB_METRICS
WHERE NAME="lock_deadlocks";
BEGIN;
SELECT b FROM t WHERE a=1 FOR UPDATE;
b
2
BEGIN;
SELECT b FROM t WHERE a=2 FOR UPDATE;
b
1
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con1_will_wait';
SELECT b FROM t WHERE a=2 FOR UPDATE;;
SET DEBUG_SYNC='now WAIT_FOR con1_will_wait';
SELECT b FROM t WHERE a=1 FOR UPDATE;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
b
1
ROLLBACK;
SELECT `count` - @deadlocks_before_test AS deadlocks_during_test
FROM INFORMATION_SCHEMA.INNODB_METRICS
WHERE NAME="lock_deadlocks";
deadlocks_during_test
1
DROP TABLE t;
