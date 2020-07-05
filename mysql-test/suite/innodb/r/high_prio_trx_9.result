CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
CREATE TABLE t2 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t2 VALUES (0);
# On connection 1
BEGIN;
SELECT * FROM t2 WHERE c1=0 FOR SHARE;
c1
0
# On connection 2
BEGIN;
SELECT * FROM t1 WHERE c1=0 FOR SHARE;
c1
0
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t2 WHERE c1=0 FOR UPDATE;
# On connection 3
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
SELECT * FROM t1 WHERE c1=0 FOR UPDATE;
c1
0
COMMIT;
# On connection 2
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
COMMIT;
# On connection 1
COMMIT;
DROP TABLE t1;
DROP TABLE t2;
