SET @old_innodb_lock_wait_timeout= @@GLOBAL.innodb_lock_wait_timeout;
SET GLOBAL innodb_lock_wait_timeout= 4;
# SCENARIO 1
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
CREATE TABLE t2 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t2 VALUES (0);
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
DELETE FROM t2;
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
DELETE FROM t1;
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con3_will_wait';
DELETE FROM t2; COMMIT;
SET DEBUG_SYNC='now WAIT_FOR con3_will_wait';
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con1_will_wait';
DELETE FROM t1; COMMIT;
COMMIT;
DROP TABLE t1;
DROP TABLE t2;
# SCENARIO 2
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
CREATE TABLE t2 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t2 VALUES (0);
CREATE TABLE t3 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t3 VALUES (0);
BEGIN;
SELECT * FROM t3 FOR UPDATE;
c1
0
BEGIN;
DELETE FROM t2;
SET DEBUG_SYNC='lock_wait_has_finished_waiting WAIT_FOR con2_can_rollback';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t3 FOR UPDATE;
SET DEBUG_SYNC='now WAIT_FOR con2_will_wait';
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
DELETE FROM t1;
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con3_will_wait';
DELETE FROM t2; COMMIT;
SET DEBUG_SYNC='now WAIT_FOR con3_will_wait';
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con1_will_wait';
DELETE FROM t1; COMMIT;
Got one of the listed errors
SET DEBUG_SYNC='now SIGNAL con2_can_rollback';
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
DROP TABLE t3;
DROP TABLE t1;
DROP TABLE t2;
SET GLOBAL innodb_lock_wait_timeout= @old_innodb_lock_wait_timeout;
