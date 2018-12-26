
--echo #
--echo # Scenario 1:
--echo # Create a small table, and add all kinds of new columns and verify
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);


# ADD COLUMN INT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 INT, ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

INSERT INTO t1(a, c1) VALUES(0, 1);
SELECT count(*) = max(a) FROM t1 WHERE c1 IS NULL;
SELECT c1 FROM t1 WHERE c1 = 1;

# ADD COLUMN INT DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c2 INT NOT NULL DEFAULT 10;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c2' AND has_default = 1;

INSERT INTO t1(a, c2) VALUES(0, 11);
SELECT count(*) = max(a) FROM t1 WHERE c2 = 10;
SELECT c2 FROM t1 WHERE c2 = 11;


# ADD COLUMN BIGINT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN d1 BIGINT, ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'd1' AND has_default = 1;

INSERT INTO t1(a, d1) VALUES(0, 1);
SELECT count(*) = max(a) FROM t1 WHERE d1 IS NULL;
SELECT d1 FROM t1 WHERE d1 = 1;

# ADD COLUMN BIGINT DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN d2 BIGINT NOT NULL DEFAULT 1234567890;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'd2' AND has_default = 1;

INSERT INTO t1(a, d2) VALUES(0, 1234);
SELECT count(*) = max(a) FROM t1 WHERE d2 = 1234567890;
SELECT d2 FROM t1 WHERE d2 = 1234;


# ADD COLUMN SMALLINT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN e1 SMALLINT, ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'e1' AND has_default = 1;

INSERT INTO t1(a, e1) VALUES(0, 1);
SELECT count(*) = max(a) FROM t1 WHERE e1 IS NULL;
SELECT e1 FROM t1 WHERE e1 = 1;

# ADD COLUMN SMALLINT DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN e2 SMALLINT NOT NULL DEFAULT 10;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'e2' AND has_default = 1;

INSERT INTO t1(a, e2) VALUES(0, 11);
SELECT count(*) = max(a) FROM t1 WHERE e2 = 10;
SELECT e2 FROM t1 WHERE e2 = 11;


# ADD COLUMN TINYINT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN f1 TINYINT, ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'f1' AND has_default = 1;

INSERT INTO t1(a, f1) VALUES(0, 1);
SELECT count(*) = max(a) FROM t1 WHERE f1 IS NULL;
SELECT f1 FROM t1 WHERE f1 = 1;

# ADD COLUMN TINYINT DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN f2 TINYINT NOT NULL DEFAULT 123;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'f2' AND has_default = 1;

INSERT INTO t1(a, f2) VALUES(0, 12);
SELECT count(*) = max(a) FROM t1 WHERE f2 = 123;
SELECT f2 FROM t1 WHERE f2 = 12;


# ADD COLUMN MEDIUMINT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN g1 MEDIUMINT, ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'g1' AND has_default = 1;

INSERT INTO t1(a, g1) VALUES(0, 1);
SELECT count(*) = max(a) FROM t1 WHERE g1 IS NULL;
SELECT g1 FROM t1 WHERE g1 = 1;

# ADD COLUMN MEDIUMINT DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN g2 MEDIUMINT NOT NULL DEFAULT 12345;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'g2' AND has_default = 1;

INSERT INTO t1(a, g2) VALUES(0, 1234);
SELECT count(*) = max(a) FROM t1 WHERE g2 = 12345;
SELECT g2 FROM t1 WHERE g2 = 1234;


# ADD COLUMN FLOAT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN h1 FLOAT, ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'h1' AND has_default = 1;

INSERT INTO t1(a, h1) VALUES(0, 1.0);
SELECT count(*) = max(a) FROM t1 WHERE h1 IS NULL;
SELECT h1 FROM t1 WHERE h1 = 1;

# ADD COLUMN FLOAT DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN h2 FLOAT NOT NULL DEFAULT 12.34;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'h2' AND has_default = 1;

INSERT INTO t1(a, h2) VALUES(0, 1.234);
SELECT count(*) = max(a) FROM t1 WHERE h2 = 12.34;
SELECT h2 FROM t1 WHERE h2 = 1.234;


# ADD COLUMN DECIMAL & DOUBLE
let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN i1 DECIMAL(5, 2), ADD COLUMN i2 double, ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE (name = 'i1' OR name = 'i2') AND has_default = 1;

INSERT INTO t1(a, i1, i2) VALUES(0, 10.10, 20.20);
SELECT count(*) = max(a) FROM t1 WHERE i1 IS NULL;
SELECT count(*) = max(a) FROM t1 WHERE i2 IS NULL;
SELECT i1 FROM t1 WHERE i1 = 10.10;
SELECT i2 FROM t1 WHERE i2 = 20.20;

# ADD COLUMN DECIMAL & DOUBLE DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN j1 DECIMAL(5, 2) NOT NULL DEFAULT 100.00, ADD COLUMN j2 double NOT NULL DEFAULT 1000.5678;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE (name = 'j1' OR name = 'j2') AND has_default = 1;

INSERT INTO t1(a, j1, j2) VALUES(0, 90.90, 1000.1234);
SELECT count(*) = max(a) FROM t1 WHERE j1 = 100.00;
SELECT count(*) = max(a) FROM t1 WHERE j2 = 1000.5678;
SELECT j1 FROM t1 WHERE j1 = 90.90;
SELECT j2 FROM t1 WHERE j2 = 1000.1234;



# ADD COLUMN BIT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN k1 BIT(8), ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'k1' AND has_default = 1;

INSERT INTO t1(a, k1) VALUES(0, b'010101');
SELECT count(*) = max(a) FROM t1 WHERE k1 IS NULL;
SELECT hex(k1) FROM t1 WHERE k1 = b'010101';

# ADD COLUMN BIT DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN k2 BIT(8) NOT NULL DEFAULT b'101010';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'k2' AND has_default = 1;

INSERT INTO t1(a, k2) VALUES(0, b'110011');
SELECT count(*) = max(a) FROM t1 WHERE k2 = b'101010';
SELECT hex(k2) FROM t1 WHERE k2 = b'110011';



# ADD COLUMN CHAR & VARCHAR
let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN l1 CHAR(50), ADD COLUMN l2 VARCHAR(100), ALGORITHM=DEFAULT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE (name = 'l1' OR name = 'l2') AND has_default = 1;

INSERT INTO t1(a, l1, l2) VALUES(0, 'ABCD EFGH', 'abcdefg hijklmn ');
SELECT count(*) = max(a) FROM t1 WHERE l2 IS NULL;
SELECT count(*) = max(a) FROM t1 WHERE l1 IS NULL;
SELECT l1 FROM t1 WHERE l1 = 'ABCD EFGH';
SELECT l2 FROM t1 WHERE l2 = 'abcdefg hijklmn ';


# ADD COLUMN CHAR & VARCHAR DEFAULT
let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN m1 CHAR(50) default 'The fox jumps over', ADD COLUMN m2 VARCHAR(50) DEFAULT 'The fox jumps over the lazy dog.';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE (name = 'm1' OR name = 'm2') AND has_default = 1;

INSERT INTO t1(a, m1, m2) VALUES(0, 'over the lazy dog', 'The lazy dog jumps over the fox.');
SELECT count(*) = max(a) FROM t1 WHERE m1 = 'The fox jumps over';
SELECT count(*) = max(a) FROM t1 WHERE m2 like 'The fox jumps%';
SELECT m1 FROM t1 WHERE m1 = 'over the lazy dog';
SELECT m2 FROM t1 WHERE m2 like '%the fox.';



# ADD COLUMN BINARY & VARBINARY
let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN n1 BINARY(10), ADD COLUMN n2 VARBINARY(10), ALGORITHM=DEFAULT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE (name = 'n1' OR name = 'n2') AND has_default = 1;

INSERT INTO t1(a, n1, n2) VALUES(0, 0x010203040506070809, 0x102030405060708090);
SELECT count(*) = max(a) FROM t1 WHERE n1 IS NULL;
SELECT count(*) = max(a) FROM t1 WHERE n2 IS NULL;
SELECT hex(n1) FROM t1 WHERE n1 = 0x01020304050607080900;
SELECT hex(n2) FROM t1 WHERE n2 = 0x102030405060708090;



# ADD COLUMN BINARY & VARBINARY DEFAULT
let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN o1 BINARY(10) DEFAULT 0x11223344, ADD COLUMN o2 VARBINARY(10) DEFAULT 0x55667788;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE (name = 'o1' OR name = 'o2') AND has_default = 1;

INSERT INTO t1(a, o1, o2) VALUES(0, 0x44332211, 0x88776655);
SELECT count(*) = max(a) FROM t1 WHERE o1 = 0x11223344000000000000;
SELECT count(*) = max(a) FROM t1 WHERE o2 = 0x55667788;
SELECT hex(o1) FROM t1 WHERE o1 = 0x44332211000000000000;
SELECT hex(o2) FROM t1 WHERE o2 = 0x88776655;



# ADD COLUMN DATETIME
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN p1 DATETIME, ALGORITHM=DEFAULT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'p1' AND has_default = 1;

INSERT INTO t1(a, p1) VALUES(0, '2017-12-31 00:00:00');
SELECT count(*) = max(a) FROM t1 WHERE p1 IS NULL;
SELECT p1 FROM t1 WHERE p1 = '2017-12-31 00:00:00';

# ADD COLUMN DATETIME DEFAULT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN p2 DATETIME NOT NULL DEFAULT '2017-12-31 01:02:03';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'p2' AND has_default = 1;

sleep 1;
SELECT count(*) = max(a) FROM t1 GROUP BY p2;
INSERT INTO t1(a, p2) VALUES(0, now());
SELECT count(*) FROM t1 GROUP BY p2;



# ADD COLUMN ENUM
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN q1 ENUM ('value1','value2','value3'), ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'q1' AND has_default = 1;

INSERT INTO t1(a, q1) VALUES(0, 1);
SELECT count(*) = max(a) FROM t1 WHERE q1 IS NULL;
SELECT q1 FROM t1 WHERE q1 = 1;


# ADD COLUMN SET
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN r1 SET ('a','b','c'), ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'r1' AND has_default = 1;

INSERT INTO t1(a, r1) VALUES(0, 'a');
SELECT count(*) = max(a) FROM t1 WHERE r1 IS NULL;
SELECT r1 FROM t1 WHERE r1 = 'a';


# ADD COLUMN BLOB & TEXT
let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN s1 BLOB, ADD COLUMN s2 TEXT, ALGORITHM=INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE (name = 's1' OR name = 's2') AND has_default = 1;

INSERT INTO t1(a, s1, s2) VALUES(0, 0x0102030405, 'abcd qwerty');
SELECT count(*) = max(a) FROM t1 WHERE s1 IS NULL;
SELECT count(*) = max(a) FROM t1 WHERE s2 IS NULL;
SELECT hex(s1) FROM t1 WHERE s1 = 0x0102030405;
SELECT s2 FROM t1 WHERE s2 = 'abcd qwerty';

# ADD COLUMN BLOB & TEXT NOT NULL
let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN u1 BLOB NOT NULL, ADD COLUMN u2 TEXT NOT NULL;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE (name = 'u1' OR name = 'u2') AND has_default = 1;

INSERT INTO t1(a, u1, u2) VALUES(0, 0x0102030405, 'abcd qwerty');
SELECT count(*) = max(a) FROM t1 WHERE u1 = '';
SELECT count(*) = max(a) FROM t1 WHERE u2 = '';
SELECT hex(u1) FROM t1 WHERE u1 = 0x0102030405;
SELECT u2 FROM t1 WHERE u2 = 'abcd qwerty';


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;



--echo #
--echo # Scenario 2:
--echo # Create a small table, add some columns instantly, along with
--echo # virtual columns
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);


let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL, ADD COLUMN d INT GENERATED ALWAYS AS ((b * 2)) VIRTUAL;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

INSERT INTO t1(a, b, c) VALUES(0, 6, 20);
SELECT * FROM t1;


let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN e VARCHAR(100) DEFAULT 'ABCD EFGH', ADD COLUMN f INT GENERATED ALWAYS AS (LENGTH(e));

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

INSERT INTO t1(a, c, e) VALUES(0, 20, 'Hello'), (0, 20, 'World'), (0, 20, 'Hello World');
SELECT * FROM t1;


let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN g VARCHAR(100) GENERATED ALWAYS AS (e), ADD COLUMN h BIGINT DEFAULT 10000, ADD COLUMN i BIGINT GENERATED ALWAYS AS (h * 2 + b);

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT * FROM t1;
INSERT INTO t1(a, b, c, h) VALUES(0, 7, 40, 2000), (0, 7, 40, 20000);
SELECT * FROM t1;


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;



--echo #
--echo # Scenario 3:
--echo # Create a small table, add some columns instantly, then change
--echo # their default values, check original default values are correct
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);


let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 100, ADD COLUMN d INT GENERATED ALWAYS AS ((c * 2 + b)) VIRTUAL, ADD COLUMN e VARCHAR(100) DEFAULT 'Hello world';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT * FROM t1;
INSERT INTO t1(a, b, c, e) VALUES(0, 6, 200, 'Good day'), (0, 7, 300, 'Good DAY');
SELECT * FROM t1;

SELECT default_value FROM information_schema.innodb_columns WHERE name = 'c' AND has_default = 1;
ALTER TABLE t1 ALTER COLUMN c SET DEFAULT 500;
# This should not change
SELECT default_value FROM information_schema.innodb_columns WHERE name = 'c' AND
 has_default = 1;

SELECT * FROM t1;
INSERT INTO t1(a, b) VALUES(0, 8), (0, 9);
SELECT * FROM t1;

SELECT default_value FROM information_schema.innodb_columns WHERE name = 'e' AND has_default = 1;
ALTER TABLE t1 ALTER COLUMN e SET DEFAULT 'HELLO MySQL!';
# This should not change
SELECT default_value FROM information_schema.innodb_columns WHERE name = 'e' AND has_default = 1;

SELECT * FROM t1;
INSERT INTO t1(a, b) VALUES(0, 10), (0, 20);
SELECT * FROM t1;


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;



--echo #
--echo # Scenario 4:
--echo # Create a small table, add some columns instantly, then do DML
--echo # on the table
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 100, ADD COLUMN d INT GENERATED ALWAYS AS ((c * 2 + b)) VIRTUAL, ADD COLUMN e VARCHAR(100) DEFAULT 'Hello world';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

UPDATE t1 SET c = 200 WHERE a > 3;
SELECT distinct(c) FROM t1;

INSERT INTO t1(b, c, e) SELECT b, c, e FROM t1;
INSERT INTO t1(b, c, e) SELECT b, c, e FROM t1;
INSERT INTO t1(b, c, e) SELECT b, c, e FROM t1;

DELETE FROM t1 WHERE c = 100;

UPDATE t1 SET c = 300;
SELECT distinct(c) FROM t1;

SELECT count(*) FROM t1;


let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN t DATETIME DEFAULT CURRENT_TIMESTAMP;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

UPDATE t1 SET e = 'Hello MySQL' WHERE a > 10;
UPDATE t1 SET e = 'Hello MySQL!!' WHERE a > 20;
SELECT distinct(e) FROM t1;

UPDATE t1 SET c = 500 WHERE e LIKE '%world%';
SELECT c, e FROM t1 GROUP BY c, e;

INSERT INTO t1(b, c, e) SELECT b, c, e FROM t1;
sleep 1;
UPDATE t1 SET t = CURRENT_TIMESTAMP WHERE a < 50;
SELECT count(t) FROM t1 GROUP BY t;
DELETE FROM t1 WHERE a < 50;
SELECT count(t) FROM t1 GROUP BY t;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;



--echo #
--echo # Scenario 5:
--echo # Create a small table, add some columns instantly, then do DDL
--echo # to build indexes
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 100, ADD COLUMN d INT GENERATED ALWAYS AS ((c * 2 + b)) VIRTUAL, ADD COLUMN e VARCHAR(100) DEFAULT 'Hello world';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

ALTER TABLE t1 ADD KEY(c);

# Replace the numbers in the output with '#' to stablize the result, after all we only care about the index picked.
--replace_column 8 # 10 #
EXPLAIN SELECT c FROM t1;

SELECT c FROM t1 WHERE c != 100;

INSERT INTO t1(b, c, e) SELECT b, c, e FROM t1;
INSERT INTO t1(b, c, e) SELECT b, c, e FROM t1;
INSERT INTO t1(b, c, e) SELECT b, c, e FROM t1;

UPDATE t1 SET e = 'Hello MySQL' WHERE a > 30;

ALTER TABLE t1 ADD KEY(e);

# Replace the numbers in the output with '#' to stablize the result, after all we only care about the index picked.
--replace_column 8 # 10 #
EXPLAIN SELECT e FROM t1;
SELECT count(e) FROM t1 WHERE e LIKE '%MySQL%';
SELECT count(e) FROM t1 WHERE e LIKE '%world%';

ALTER TABLE t1 DROP PRIMARY KEY, ADD PRIMARY KEY(a, c);

SELECT a, c, e FROM t1 WHERE a > 25 AND a < 40;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;



--echo #
--echo # Scenario 6:
--echo # Create a small table, add some columns instantly, then do DML
--echo # on the table, and some simple rollback
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 100, ADD COLUMN d INT GENERATED ALWAYS AS ((c * 2 + b)) VIRTUAL, ADD COLUMN e VARCHAR(100) DEFAULT 'Hello world';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

START TRANSACTION;

INSERT INTO t1(a, b) VALUES(0, 6);

SELECT * FROM t1;

ROLLBACK;

SELECT * FROM t1;


START TRANSACTION;

UPDATE t1 SET c = 500 WHERE a = 1;

UPDATE t1 SET b = 1000 WHERE a = 2;

SELECT a, b, c FROM t1 WHERE a = 1 OR a = 2;

ROLLBACK;

SELECT a, b, c FROM t1;


START TRANSACTION;

DELETE FROM t1 WHERE a < 5;

INSERT INTO t1(a, b) VALUES(0, 6);

SELECT * FROM t1;

ROLLBACK;

SELECT * FROM t1;


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


# One interesting pessimistic update rollback

--eval CREATE TABLE t1(id INT PRIMARY KEY, c1 VARCHAR(4000), c2 VARCHAR(4000), c3 VARCHAR(1000)) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(1, repeat('a', 4000), repeat('b', 4000), repeat('c', 1));

SELECT id, length(c1), length(c2), length(c3) FROM t1;

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c4 VARCHAR(500) NOT NULL DEFAULT 'dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT id, length(c1), length(c2), length(c3), length(c4) FROM t1;

START TRANSACTION;

UPDATE t1 SET c1 = repeat('x', 200) WHERE id = 1;

ROLLBACK;

SELECT id, length(c1), length(c2), length(c3), length(c4) FROM t1;

START TRANSACTION;

UPDATE t1 SET c4 = 'x' WHERE id = 1;

ROLLBACK;

SELECT id, length(c1), length(c2), length(c3), length(c4) FROM t1;

DROP TABLE t1;



--echo #
--echo # Scenario 7:
--echo # Confirm some ADD COLUMN are instant, some are not
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN after LAST should be INSTANT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 100 AFTER b;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

# ADD COLUMN in the middle should not be instant
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN d INT NOT NULL DEFAULT 100 AFTER b;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;

# ADD COLUMN with ADD KEY should not be instant
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN e INT NOT NULL DEFAULT 100, ADD KEY(e);

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;

# FORCE key word should not be instant
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN f INT NOT NULL DEFAULT 100, FORCE;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;

# ALGORITHM=INPLACE should not be instant, verify again here
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN g INT NOT NULL DEFAULT 100, ALGORITHM=INPLACE;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT count(*) AS `Expect 0` FROM information_schema.innodb_columns WHERE has_default = 1;


CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT, c TEXT, FULLTEXT(c)) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1, 'Hello'), (0, 2, 'HELLO'), (0, 3, 'World'), (0, 4, 'Hello world'), (0, 5, 'HELLO WORLD');

# ADD COLUMN to FULLTEXT table should not be instant
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN d INT NOT NULL DEFAULT 100 AFTER b;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

DROP TABLE t1;


--echo #
--echo # Scenario 8:
--echo # Check FK constraints on instantly added columns
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

CREATE TABLE t2(a INT NOT NULL PRIMARY KEY, b INT, c INT, KEY(c));

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

INSERT INTO t2 VALUES(1, 2, 3), (2, 3, 4), (3, 4, 5);

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 3;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

UPDATE t1 SET c = 4 WHERE a = 2;
UPDATE t1 SET c = 5 WHERE a = 3;

ALTER TABLE t1 ADD KEY(c);

ALTER TABLE t2 ADD FOREIGN KEY (c) REFERENCES t1(c) ON UPDATE CASCADE, ALGORITHM = COPY;

UPDATE t1 SET c = 10 WHERE a = 1;
SELECT c FROM t2;

UPDATE t1 SET c = 10;
SELECT c FROM t2;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t2;
DROP TABLE t1;



--echo #
--echo # Scenario 9:
--echo # Instant ADD COLUMN on partitioned table, only simple test here
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format PARTITION BY HASH(a) PARTITIONS 3;

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7), (0, 8);

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 5;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT * FROM t1;

INSERT INTO t1 VALUES(0, 9, 10), (0, 10, 20);

SELECT * FROM t1 WHERE b > 8;

UPDATE t1 SET c = 8 WHERE a = 1 OR a = 3 OR a = 5 OR a = 7;

SELECT * FROM t1;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


# Check instant ADD COLUMN and then common ALTER TABLE and then instant ADD COLUMN

--eval CREATE TABLE t1 (a INT, b INT) ROW_FORMAT=$row_format PARTITION BY HASH(a) PARTITIONS 2;

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 5;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

ALTER TABLE t1 CHANGE COLUMN c c1 INT;

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 5;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

DROP TABLE t1;


--echo #
--echo # Scenario 10:
--echo # EXCHANGE PARTITION is not allowed if either is instant
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format;

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7), (0, 8);

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 5;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

--eval CREATE TABLE t2 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT, c INT NOT NULL DEFAULT 5) ROW_FORMAT=$row_format PARTITION BY RANGE (a) (PARTITION p1 VALUES LESS THAN (10), PARTITION p2 VALUES LESS THAN (20), PARTITION p3 VALUES LESS THAN (30));

# t1 is instant table, so should fail
--error ER_PARTITION_EXCHANGE_DIFFERENT_OPTION
ALTER TABLE t2 EXCHANGE PARTITION p1 WITH TABLE t1;

# Make t2 an instant table
ALTER TABLE t2 ADD COLUMN d INT;

# Make t1 a non-instant table
ALTER TABLE t1 ADD COLUMN d INT, ALGORITHM=INPLACE;

# t2 is instant table, so should fail
--error ER_PARTITION_EXCHANGE_DIFFERENT_OPTION
ALTER TABLE t2 EXCHANGE PARTITION p1 WITH TABLE t1;

OPTIMIZE TABLE t2;
SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;
# Now this should succeed
ALTER TABLE t2 EXCHANGE PARTITION p1 WITH TABLE t1;

DROP TABLE t1, t2;


--echo #
--echo # Scenario 11:
--echo # PRIMARY KEY with more than one column, at least to verify it works with REDUDANT
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL, b INT NOT NULL, PRIMARY KEY(a, b)) ROW_FORMAT=$row_format;

INSERT INTO t1 VALUES(0, 1), (1, 2), (2, 3), (3, 4);

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 5;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT * FROM t1;
UPDATE t1 SET c = b WHERE b <= 2;

CHECK TABLE t1;
SELECT * FROM t1;

DROP TABLE t1;


--echo #
--echo # Scenario 12:
--echo # Mix ALTER PARTITION and ALTER TABLE ... INPLACE. This is to check if first partition is not
--echo # instant after ALTER PARTITION, will the metadata be copied correctly
--echo #

--eval CREATE TABLE t1 (col1 INT, col2 INT, col3 INT, col4 TEXT) ENGINE = InnoDB PARTITION BY RANGE(col1 * 2) ( PARTITION p0 VALUES LESS THAN (128), PARTITION p1 VALUES LESS THAN (256) , PARTITION p2 VALUES LESS THAN (384) , PARTITION p3 VALUES LESS THAN MAXVALUE);

INSERT INTO t1 VALUES(1, 2, 3, 'abcdefg'), (100, 200, 300, 'qwerty'), (200, 300, 400, 'asdfg');

let $new_cols = 2;
let $instant_add_column = ALTER TABLE t1 ALGORITHM DEFAULT, ADD COLUMN col5 VARCHAR(500), ADD COLUMN col6 TEXT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT count(*) AS `Expect 4` FROM information_schema.innodb_tables WHERE instant_cols != 0;

ALTER TABLE t1 ALGORITHM INPLACE, REORGANIZE PARTITION p0 INTO (PARTITION p0_a VALUES LESS THAN (64), PARTITION p0_b VALUES LESS THAN (128));
SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;

ALTER TABLE t1 ALGORITHM DEFAULT, ADD KEY idx4(col4(10));
SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;

ALTER TABLE t1 ALGORITHM DEFAULT, LOCK EXCLUSIVE, REORGANIZE PARTITION p0_a, p0_b INTO (PARTITION p0 VALUES LESS THAN (128) TABLESPACE innodb_file_per_table);
SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;

ALTER TABLE t1 ADD KEY idx3(col3);
SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;

SELECT * FROM t1;

DROP TABLE t1;


--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format PARTITION BY HASH(a) PARTITIONS 3;

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7), (0, 8);

let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 5;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;

ALTER TABLE t1 ADD PARTITION PARTITIONS 10;
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;

ALTER TABLE t1 ADD KEY(b);
SELECT count(*) AS `Expect 0` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;

SELECT * FROM t1;

DROP TABLE t1;



--echo #
--echo # Scenario 13:
--echo # Create a table with a two level clustered index, do instant ADD COLUMN, then the non-leaf node
--echo # should be parsed correctly
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT, d INT NOT NULL, b BLOB NOT NULL, c VARCHAR(87), INDEX(d), INDEX(a ASC), PRIMARY KEY (a, d DESC, c DESC)) ROW_FORMAT=$row_format PARTITION BY LINEAR KEY(c) PARTITIONS 9;

INSERT INTO t1(d, b, c) VALUES(1, 2, 'aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffgggggggggghhhhhhhhhhjjjjjjj');
INSERT INTO t1(d, b, c) VALUES(2, 3, 'aaaaaaaaaahhhhhhhhhhbbbbbbbbbbccccccccccddddddddddeeeeeeeeeeffffffffffggggggggggjjjjjjj');
INSERT INTO t1(d, b, c) VALUES(3, 4, 'bbbbbbbbbbaaaaaaaaaahhhhhhhhhhccccccccccddddddddddeeeeeeeeeeffffffffffggggggggggjjjjjjj');
INSERT INTO t1(d, b, c) VALUES(4, 5, 'eeeeeeeeeehhhhhhhhhhbbbbbbbbbbccccccccccddddddddddaaaaaaaaaaffffffffffggggggggggjjjjjjj');
INSERT INTO t1(d, b, c) VALUES(5, 6, 'aaaaaaaaaahhhhhhhhhhbbbbbbbbbbddddddddddcccccccccceeeeeeeeeeffffffffffggggggggggjjjjjjj');
INSERT INTO t1(d, b, c) VALUES(6, 7, 'cccccccccchhhhhhhhhhbbbbbbbbbbaaaaaaaaaaddddddddddeeeeeeeeeeffffffffffggggggggggjjjjjjj');

INSERT INTO t1(d, b, c) SELECT d, b, c FROM t1;
INSERT INTO t1(d, b, c) SELECT d, b, c FROM t1;
INSERT INTO t1(d, b, c) SELECT d, b, c FROM t1;
INSERT INTO t1(d, b, c) SELECT d, b, c FROM t1;
INSERT INTO t1(d, b, c) SELECT d, b, c FROM t1;
INSERT INTO t1(d, b, c) SELECT d, b, c FROM t1;

ALTER TABLE t1 ADD COLUMN nc086 BIGINT NOT NULL FIRST, ALGORITHM=INPLACE, LOCK=DEFAULT;
CHECK TABLE t1;

ALTER TABLE t1 ADD COLUMN nc082 TINYTEXT;

CHECK TABLE t1;

SELECT COUNT(*) FROM t1;

DROP TABLE t1;


--echo #
--echo # Scenario 14:
--echo # Create a small table, and add GIS kinds of new columns and verify
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN POINT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 POINT, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

INSERT INTO t1(a, c1) VALUES(0, ST_PointFromText('POINT(10 10)'));
SELECT count(*) = max(a) FROM t1 WHERE c1 IS NULL;

# ADD COLUMN LINESTRING
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN d1 LINESTRING, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'd1' AND has_default = 1;

INSERT INTO t1(a, d1) VALUES(0, ST_LineFromText('LINESTRING(0 0,0 10,10 0)'));
SELECT count(*) = max(a) FROM t1 WHERE d1 IS NULL;

# ADD COLUMN POLYGON
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN e1 POLYGON, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'e1' AND has_default = 1;

INSERT INTO t1(a, e1) VALUES(0, ST_PolygonFromText('POLYGON((10 10,20 10,20 20,10 20,10 10))'));
SELECT count(*) = max(a) FROM t1 WHERE e1 IS NULL;

# ADD COLUMN MULTIPOINT
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN f1 MULTIPOINT, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'f1' AND has_default = 1;

INSERT INTO t1(a, f1) VALUES(0, ST_MultiPointFromText('MULTIPOINT(0 0,10 10,10 20,20 20)'));
SELECT count(*) = max(a) FROM t1 WHERE f1 IS NULL;

# ADD COLUMN MULTILINESTRING
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN g1 MULTILINESTRING, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'g1' AND has_default = 1;

INSERT INTO t1(a, g1) VALUES(0, ST_MultiLineStringFromText('MULTILINESTRING((10 48,10 21,10 0),(16 0,16 23,16 48))'));
SELECT count(*) = max(a) FROM t1 WHERE g1 IS NULL;

# ADD COLUMN MULTIPOLYGON
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN h1 MULTIPOLYGON, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'h1' AND has_default = 1;

INSERT INTO t1(a, h1) VALUES(0, ST_MultiPolygonFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73 9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))'));
SELECT count(*) = max(a) FROM t1 WHERE h1 IS NULL;


# ADD COLUMN GEOMETRYCOLLECTION
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN i1 GEOMETRYCOLLECTION, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'i1' AND has_default = 1;

INSERT INTO t1(a, i1) VALUES(0, ST_GeomCollFromText('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0,10 10))'));
SELECT count(*) = max(a) FROM t1 WHERE i1 IS NULL;


# ADD COLUMN GEOMETRY
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN j1 GEOMETRY, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'j1' AND has_default = 1;

INSERT INTO t1(a, j1) VALUES(0, ST_GeomCollFromText('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0,10 10))'));
SELECT count(*) = max(a) FROM t1 WHERE j1 IS NULL;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;

--echo #
--echo # Scenario 15:
--echo # Create a small table, and add JSON columns and verify
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN JSON
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 JSON, ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

INSERT INTO t1(a, c1) VALUES(0, '{"key1": "value1", "key2": "value2"}');
SELECT count(*) = max(a) FROM t1 WHERE c1 IS NULL;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


--echo #
--echo # Scenario 16:
--echo # Create a small table, and add INSTANT columns and verify with trigger
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format
--eval CREATE TABLE t2 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);
INSERT INTO t2 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'aaaa', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

# TRIGGERS AFTER INSERT INTO ADDED COLUMN
CREATE TRIGGER t1_ai AFTER INSERT ON t1 FOR EACH ROW
   INSERT INTO t2 VALUES(0,6);

INSERT INTO t1(a, c1) VALUES(0, 'bbbb');
SELECT count(*) = max(a) FROM t1 WHERE c1='aaaa';
SELECT c1 FROM t1 WHERE c1 = 'bbbb';
DROP TRIGGER t1_ai;


# ADD COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t2 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'cccc', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

# INSERT ROW INTO ADDED COLUMN USING TRIGGER
CREATE TRIGGER t2_ai AFTER INSERT ON t2 FOR EACH ROW
   INSERT INTO t1(a,c1) VALUES(0,'eeee');

INSERT INTO t2(a, c1) VALUES(0, 'dddd');
SELECT count(*) = max(a) FROM t2 WHERE c1='cccc';
SELECT c1 FROM t2 WHERE c1 = 'dddd';
DROP TRIGGER t2_ai;


CHECK TABLE t1;
CHECK TABLE t2;

SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;

DROP TABLE t1,t2;


--echo #
--echo # Scenario 17:
--echo # Create a small table, and add INSTANT columns and verify with storedprocedure
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'aaaa', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

# STORED PROCEDURE
CREATE PROCEDURE p1() INSERT INTO t1(a,c1) VALUES(0, 'bbbb');
CALL p1();

SELECT count(*) = max(a) FROM t1 WHERE c1='aaaa';
SELECT c1 FROM t1 WHERE c1 = 'bbbb';
DROP PROCEDURE p1;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


--echo #
--echo # Scenario 18:
--echo # Create a small table, and add INSTANT columns and verify with view
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'aaaa', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

# CREATE VIEW
CREATE VIEW v1 AS SELECT * FROM t1;

INSERT INTO t1(a, c1) VALUES(0, 'bbbb');
SELECT count(*) = max(a) FROM t1 WHERE c1='aaaa';
SELECT c1 FROM t1 WHERE c1 = 'bbbb';
SELECT * FROM v1;
DROP VIEW v1;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


--echo #
--echo # Scenario 19:
--echo # Create a small table, and add INSTANT columns and drop it and verify
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'aaaa', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

INSERT INTO t1(a, c1) VALUES(0, 'bbbb');
SELECT count(*) = max(a) FROM t1 WHERE c1='aaaa';
SELECT c1 FROM t1 WHERE c1 = 'bbbb';

# DROP COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 DROP COLUMN c1;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


--echo #
--echo # Scenario 20:
--echo # Create a small table, and add INSTANT columns and rename table
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'aaaa', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

ALTER TABLE t1 RENAME t2;

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;
INSERT INTO t2(a, c1) VALUES(0, 'bbbb');
SELECT count(*) = max(a) FROM t2 WHERE c1='aaaa';
SELECT c1 FROM t2 WHERE c1 = 'bbbb';

CHECK TABLE t2;

SHOW CREATE TABLE t2;

DROP TABLE t2;


--echo #
--echo # Scenario 21:
--echo # Create a small table, and add INSTANT columns and change its data type INSTANTly won't work
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'aaaa', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;
INSERT INTO t1(a, c1) VALUES(0, 'bbbb');
SELECT count(*) = max(a) FROM t1 WHERE c1='aaaa';
SELECT c1 FROM t1 WHERE c1 = 'bbbb';

# change added column datatype should not be instant
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 CHANGE c1 c2 CHAR(10) NOT NULL DEFAULT 'cccc';

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


--echo #
--echo # Scenario 22:
--echo # Create a small table, and add INSTANT columns and create hash,btree multi column index and verify
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'aaaa', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

# CREATE INDEX
CREATE INDEX id1 ON t1(c1) USING BTREE;
CREATE INDEX id2 ON t1(c1) USING HASH;

INSERT INTO t1(a, c1) VALUES(0, 'bbbb');
SELECT count(*) = max(a) FROM t1 WHERE c1='aaaa';
SELECT c1 FROM t1 WHERE c1 = 'bbbb';

DROP INDEX id1 ON t1;
DROP INDEX id2 ON t1;

# ADD ANOTHER COLUMN
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c2 VARCHAR(10) NOT NULL DEFAULT 'cccc', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c2' AND has_default = 1;

# CREATE INDEX
CREATE INDEX id1 ON t1(c1 ASC,c2 DESC) USING BTREE;

INSERT INTO t1(a, c2) VALUES(0, 'dddd');
SELECT count(*) = max(a) FROM t1 WHERE c1='cccc';
SELECT c1 FROM t1 WHERE c1 = 'dddd';

# RENAME INDEX
ALTER TABLE t1 RENAME INDEX id1 TO id2;

DROP INDEX id2 ON t1;

CHECK TABLE t1;

SHOW CREATE TABLE t1;

DROP TABLE t1;


--echo #
--echo # Scenario 23:
--echo # Create a small table, and add INSTANT columns and perform table join operation
--echo #

--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format
--eval CREATE TABLE t2 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format

INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);
INSERT INTO t2 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);

# ADD COLUMN TO TABLE t1
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t1 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'aaaa', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

INSERT INTO t1(a, c1) VALUES(0, 'cccc');
SELECT count(*) = max(a) FROM t1 WHERE c1='aaaa';
SELECT c1 FROM t1 WHERE c1 = 'cccc';

# ADD COLUMN TO TABLE t2
let $new_cols = 1;
let $instant_add_column = ALTER TABLE t2 ADD COLUMN c1 VARCHAR(10) NOT NULL DEFAULT 'bbbb', ALGORITHM = INSTANT;

--source ../mysql-test/suite/innodb/include/instant_add_column_exec_and_verify.inc

SELECT name, default_value FROM information_schema.innodb_columns WHERE name = 'c1' AND has_default = 1;

INSERT INTO t2(a, c1) VALUES(0, 'cccc');
SELECT count(*) = max(a) FROM t2 WHERE c1='bbbb';
SELECT c1 FROM t2 WHERE c1 = 'cccc';

SELECT * FROM t1 JOIN t2 ON t1.c1=t2.c1;

CHECK TABLE t1;
CHECK TABLE t2;

SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;

DROP TABLE t1;
DROP TABLE t2;
