CREATE TABLE t1 (a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1);
XA START 'x';
UPDATE t1 set a=2;
XA END 'x';
XA PREPARE 'x';
call mtr.add_suppression("Found 1 prepared XA transactions");
# Kill and restart
SELECT * FROM t1 LOCK IN SHARE MODE;
# restart
SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
SELECT * FROM t1;
a
2
XA ROLLBACK 'x';
SELECT * FROM t1;
a
1
DROP TABLE t1;
#
# Bug #31437127 - ERROR LOG REPORTS INVALID (OLD?) TABLE OR DATABASE NAME:
# ERROR] [MY-010520] [Server] Invalid (old?) table or database name 't1#p#p1'
#
call mtr.add_suppression("Found .* prepared XA transactions");
CREATE TABLE t1(a INT PRIMARY KEY, b INT) PARTITION BY HASH(a) PARTITIONS 4;
XA BEGIN '123';
INSERT INTO t1 VALUES(1,2);
XA END '123';
XA PREPARE '123';
# restart
XA RECOVER;
formatID	gtrid_length	bqual_length	data
1	3	0	123
XA COMMIT '123';
DROP TABLE t1;
