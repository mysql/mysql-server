CREATE TABLE t1(a int primary key);
CREATE TABLE t2(a int primary key);
BEGIN;
#
# Insert values on table and savepoints
#
SAVEPOINT S1;
INSERT INTO t1 values(3);
INSERT INTO t1 values(4);
SAVEPOINT S2;
INSERT INTO t1 values(5);
INSERT INTO t1 values(6);
SAVEPOINT S3;
SAVEPOINT S4;
#
# RELEASE S4
#
RELEASE SAVEPOINT S4;
INSERT INTO t1 values(7);
SELECT * FROM t1;
a
3
4
5
6
7
#
# ROLLBACK TO S2
#
ROLLBACK TO S2;
#
# Confirm S3 was removed when rollbacking to S2
#
ROLLBACK TO S3;
ERROR 42000: SAVEPOINT S3 does not exist
SELECT * FROM t1;
a
3
4
INSERT INTO t1 values(7);
SELECT * FROM t1;
a
3
4
7
#
# Confirm S2 exists after ROLLBACK TO S2
#
ROLLBACK TO S2;
SELECT * FROM t1;
a
3
4
COMMIT;
#
# Check the multitable update on 'rollback to savepoint'
#
BEGIN;
INSERT INTO t1 values (5);
SAVEPOINT S1;
DELETE FROM t1;
INSERT INTO t1 values (1);
INSERT INTO t2 SELECT * FROM t1;
UPDATE t1,t2 SET t1.a=3, t2.a=3;
SELECT * FROM t1;
a
3
SELECT * FROM t2;
a
3
ROLLBACK TO SAVEPOINT S1;
INSERT INTO t2 SELECT * FROM t1;
COMMIT;
SELECT * FROM t1;
a
3
4
5
SELECT * FROM t2;
a
3
4
5
DROP TABLE t1;
DROP TABLE t2;
