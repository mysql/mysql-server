CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);

# On connection con1
START TRANSACTION;
UPDATE t1 SET c1=1 WHERE c1=0;

# On connection con2
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET c1=2 WHERE c1=0;
SET DEBUG='+d,crash_commit_after';
COMMIT;
ERROR HY000: Lost connection to MySQL server during query

# On connection default
# Value 2 is expected as server crash on high prio txn (crash_commit_after point)
SELECT * FROM t1;
c1
2
DROP TABLE t1;
# Case : crash before commit on high prio transaction
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);

# On connection con1
START TRANSACTION;
UPDATE t1 SET c1=1 WHERE c1=0;

# On connection con2
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET c1=2 WHERE c1=0;
# Kill and restart
# Value 0 is expected as server crash on high prio txn (crash_commit_before point)
SELECT * FROM t1;
c1
0
DROP TABLE t1;
