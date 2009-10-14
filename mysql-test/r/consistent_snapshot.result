DROP TABLE IF EXISTS t1;
# Establish connection con1 (user=root)
# Establish connection con2 (user=root)
### Test 1:
### - While a consistent snapshot transaction is executed,
###   no external inserts should be visible to the transaction.
# Switch to connection con1
CREATE TABLE t1 (a INT) ENGINE=innodb;
START TRANSACTION WITH CONSISTENT SNAPSHOT;
# Switch to connection con2
INSERT INTO t1 VALUES(1);
# Switch to connection con1
SELECT * FROM t1;
a
COMMIT;
### Test 2:
### - For any non-consistent snapshot transaction, external
###   committed inserts should be visible to the transaction.
DELETE FROM t1;
START TRANSACTION;
# Switch to connection con2
INSERT INTO t1 VALUES(1);
# Switch to connection con1
SELECT * FROM t1;
a
1
COMMIT;
### Test 3:
### - Bug#44664: valgrind warning for COMMIT_AND_CHAIN and ROLLBACK_AND_CHAIN
###   Chaining a transaction does not retain consistency level.
START TRANSACTION WITH CONSISTENT SNAPSHOT;
DELETE FROM t1;
COMMIT WORK AND CHAIN;
# Switch to connection con2
INSERT INTO t1 VALUES(1);
# Switch to connection con1
SELECT * FROM t1;
a
1
COMMIT;
# Switch to connection default + close connections con1 and con2
DROP TABLE t1;
