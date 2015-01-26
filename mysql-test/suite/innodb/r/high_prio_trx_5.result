CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY, c2 INT NOT NULL, c3 INT NOT NULL) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0, 0, 0);

# On connection 1
START TRANSACTION READ ONLY;
SELECT c1, c2 FROM t1 WHERE c1=0 LOCK IN SHARE MODE;
c1	c2
0	0

# On connection 2
START TRANSACTION;
UPDATE t1 SET c1=2 WHERE c1=0 AND c2=0;

# On connection 3
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
SET @con2_id = <con2_id>;
SET DEBUG_SYNC='rec_lock_add_to_waitq SIGNAL signal1 WAIT_FOR waiting1';
UPDATE t1 SET c1=3 WHERE c1=0;;

# On connection 1
SET DEBUG_SYNC='now WAIT_FOR signal1';
SET DEBUG_SYNC='now SIGNAL waiting1';
COMMIT;

# On connection 3
COMMIT;

# On connection 2
COMMIT;
include/assert.inc ['There is a 3.0.0 in t1']
include/assert.inc ['There is no 0.0.0 in t1']
include/assert.inc ['There is no 2 in t1.c1']
DROP TABLE t1;
