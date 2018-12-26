CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY, c2 INT NOT NULL, c3 INT NOT NULL) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0, 0, 0);

# On connection 1
START TRANSACTION;
SELECT c1, c2 FROM t1 WHERE c1=0 LOCK IN SHARE MODE;
c1	c2
0	0

# On connection 2
START TRANSACTION;
SELECT c3, c2 FROM t1 WHERE c1=0 LOCK IN SHARE MODE;
c3	c2
0	0

# On connection 3
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET c1=3 WHERE c1=0;
COMMIT;

# On connection 1
COMMIT;
ERROR HY000: Got error 149 - 'Lock deadlock; Retry transaction' during COMMIT

# On connection 2
COMMIT;
ERROR HY000: Got error 149 - 'Lock deadlock; Retry transaction' during COMMIT
include/assert.inc ['There is a 3.0.0 in t1']
include/assert.inc ['There is no 0.0.0 in t1']
DROP TABLE t1;
