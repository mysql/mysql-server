#
# 1. Apply some data on server.
#
CREATE TABLE t1 (c1 INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (3);
#
# 2. Check that a SQL file with already logged transactions
#    is correctly skipped when is replayed.
#
FLUSH LOGS;
#
# 3. Clean up.
#
DROP TABLE t1;
