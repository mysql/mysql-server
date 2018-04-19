#
# Bug#21869656 UNDO LOG DOES NOT CONTAIN ENOUGH INFORMATION
# ON INDEXED VIRTUAL COLUMNS
#
CREATE TABLE t1 (a INT, b INT,
a1 INT GENERATED ALWAYS AS (a) VIRTUAL, INDEX(a1)
) ENGINE=InnoDB;
INSERT INTO t1 (a,b) VALUES(1,1);
CREATE TABLE t0 (a INT) ENGINE=InnoDB;
BEGIN;
SELECT * FROM t0;
a
UPDATE t1 SET a=0;
ALTER TABLE t1 DROP COLUMN a1, ALGORITHM=INPLACE;
ALTER TABLE t1 ADD COLUMN b1 INT GENERATED ALWAYS AS (b) VIRTUAL, ADD
INDEX(b1),
ALGORITHM=INPLACE;
COMMIT;
UPDATE t1 SET a=1;
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SELECT b1 FROM t1;
b1
1
ALTER TABLE t1
ADD COLUMN a1 INT GENERATED ALWAYS AS (a) VIRTUAL,
ADD COLUMN a2 INT GENERATED ALWAYS AS (a + b) VIRTUAL,
ADD COLUMN a3 INT GENERATED ALWAYS AS (a - b) VIRTUAL,
ADD COLUMN a4 INT GENERATED ALWAYS AS (a - b) VIRTUAL,
ADD INDEX(a1), ADD INDEX(a2), ADD INDEX(a3), ALGORITHM=INPLACE;
CREATE TABLE t2 (
a BLOB,
b BLOB,
c BLOB GENERATED ALWAYS AS (CONCAT(a,b)) VIRTUAL,
h VARCHAR(10) DEFAULT NULL
) ENGINE=InnoDB;
INSERT INTO t2 VALUES (REPEAT('g', 16000), REPEAT('x', 16000), DEFAULT, 'kk');
INSERT INTO t2 VALUES (REPEAT('a', 16000), REPEAT('b', 16000), DEFAULT, 'mm');
CREATE INDEX idx ON t2(c(100));
INSERT INTO t1 (a, b) VALUES(1,1);
BEGIN;
SELECT * FROM t0;
a
UPDATE t1 SET a=0;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
UPDATE t1 SET b=0;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
ALTER TABLE t1 DROP COLUMN a3, ALGORITHM=INPLACE;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t1 SET a=2;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
ALTER TABLE t1 DROP COLUMN a2, ALGORITHM=INPLACE;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t1 SET b=3;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
ALTER TABLE t1 ADD COLUMN b2 INT GENERATED ALWAYS AS (b) VIRTUAL,
ADD INDEX(b2), ALGORITHM=INPLACE;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t1 SET b=9;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
ALTER TABLE t1 ADD COLUMN b3 INT GENERATED ALWAYS AS (a) VIRTUAL,
ADD INDEX(b3), ALGORITHM=INPLACE;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t1 SET b=10;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
ALTER TABLE t2 DROP COLUMN c;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t2 SET a = REPEAT('s', 6000) WHERE a like 'aaa%';
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
ALTER TABLE t2 ADD COLUMN x1 BLOB GENERATED ALWAYS AS (CONCAT(a,b)) VIRTUAL,
ADD COLUMN x2 BLOB GENERATED ALWAYS AS (CONCAT(a,b)) VIRTUAL,
ADD INDEX(x1(100), x2(120)), ADD INDEX (x1(20));
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t1 SET a=5;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
UPDATE t2 SET a = REPEAT('m', 16000) WHERE a like 'sss%';
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
ALTER TABLE t1 DROP COLUMN b2, ALGORITHM=INPLACE;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t1 SET a=6;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
ALTER TABLE t2 DROP COLUMN x1, DROP COLUMN x2, ALGORITHM=INPLACE;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t2 SET a = REPEAT('x', 1000) WHERE a like 'mmm%';
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
ALTER TABLE t1 DROP INDEX b3;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
UPDATE t1 SET a=100;
affected rows: 2
info: Rows matched: 2  Changed: 2  Warnings: 0
COMMIT;
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SELECT b1 FROM t1;
b1
10
10
SELECT * FROM t1;
a	b	b1	a1	a4	b3
100	10	10	100	90	100
100	10	10	100	90	100
CHECK TABLE t2;
Table	Op	Msg_type	Msg_text
test.t2	check	status	OK
DROP TABLE t2, t1, t0;
CREATE TABLE t1 (a VARCHAR(30), b INT, a2 VARCHAR(30) GENERATED ALWAYS AS (a) VIRTUAL);
CREATE INDEX idx ON t1(a2(10), b, a2(20));
ERROR 42S21: Duplicate column name 'a2'
DROP TABLE t1;
