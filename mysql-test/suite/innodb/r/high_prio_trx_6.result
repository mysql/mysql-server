CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
INSERT INTO t1 VALUES (10);

# On connection 1
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET c1=1 WHERE c1=0;

# On connection 2
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET c1=12 WHERE c1=10;

# On connection 1
# Send
UPDATE t1 SET c1=11 WHERE c1=10;;

# On connection 2
UPDATE t1 SET c1=2 WHERE c1=0;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction

# On connection 1
# Reap UPDATE t1 SET c1=11 WHERE c1=10
COMMIT;
include/assert.inc ['There is a 1 in t1']
include/assert.inc ['There is no 2 in t1']
include/assert.inc ['There is no 0 in t1']
DROP TABLE t1;
