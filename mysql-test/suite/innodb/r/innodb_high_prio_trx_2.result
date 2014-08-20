CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);

# On connection 1
START TRANSACTION;
SELECT * FROM t1 WHERE c1=0 FOR UPDATE;
c1
0

# On connection 2
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET c1=2 WHERE c1=0;
COMMIT;

# On connection 1
UPDATE t1 SET c1=1 WHERE c1=0;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
include/assert.inc ['There is a 2 in t1']
include/assert.inc ['There is no 1 in t1']
include/assert.inc ['There is no 0 in t1']
DROP TABLE t1;
