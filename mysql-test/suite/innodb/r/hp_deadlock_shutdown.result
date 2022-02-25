SET GLOBAL innodb_deadlock_detect = 'OFF';
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
CREATE TABLE t2 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t2 VALUES (0);
CREATE TABLE t3 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t3 VALUES (0);
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
DELETE FROM t1;
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
DELETE FROM t2;
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
DELETE FROM t3;
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con3_will_wait';
DELETE FROM t2; COMMIT;
SET DEBUG_SYNC='now WAIT_FOR con3_will_wait';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con2_will_wait';
DELETE FROM t1; COMMIT;
SET DEBUG_SYNC='now WAIT_FOR con2_will_wait';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con1_will_wait';
DELETE FROM t3; COMMIT;
SET DEBUG_SYNC='now WAIT_FOR con1_will_wait';
# restart
SELECT * FROM t1;
c1
0
DROP TABLE t1;
SELECT * FROM t2;
c1
0
DROP TABLE t2;
SELECT * FROM t3;
c1
0
DROP TABLE t3;
