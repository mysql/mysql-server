CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
# On connection 1
BEGIN;
SELECT * FROM t1 WHERE c1=0 FOR SHARE;
c1
0
# On connection 2
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t1 WHERE c1=0 FOR UPDATE;
# On connection 3
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
SELECT * FROM t1 WHERE c1=0 FOR SHARE;
c1
0
COMMIT;
# On connection 1
COMMIT;
# On connection 2
c1
0
COMMIT;
DROP TABLE t1;
