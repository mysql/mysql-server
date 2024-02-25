--source include/have_debug.inc
SET SESSION debug= '+d,skip_dd_table_access_check';

--echo #
--echo # Tests column with functions set as DEFAULT. Test needs debug mode
--echo # in order to be able to check for the changes in DD.
--echo #

--echo #
--echo # CREATE TABLE - column with func as DEFAULT then test DROP column.
--echo #
CREATE TABLE t1 (i int,a TIMESTAMP DEFAULT CURRENT_TIMESTAMP, b JSON DEFAULT (JSON_OBJECT("key", i)));
SELECT CONCAT(t.name, ".", c.name) as col_name,
              c.default_option, c.default_value_utf8
FROM mysql.tables AS t JOIN mysql.columns as c ON t.id = c.table_id
WHERE t.name = 't1' ORDER BY c.id;

SET timestamp= 1038401397;
INSERT INTO t1(i) VALUES (1);
INSERT INTO t1(i, b) VALUES (2, DEFAULT);
INSERT INTO t1(i, b) VALUES (3, JSON_OBJECT("key", 3));
SELECT * FROM t1;
ALTER TABLE t1 DROP COLUMN b;
SELECT CONCAT(t.name, ".", c.name) as col_name,
              c.default_option, c.default_value_utf8
FROM mysql.tables AS t JOIN mysql.columns as c ON t.id = c.table_id
WHERE t.name = 't1' ORDER BY c.id;
DROP TABLE t1;

--echo #
--echo # Test ALTER table ADD column with DEFAULT
--echo #
CREATE TABLE t1 (i int);
INSERT INTO t1(i) VALUES (1),(2);
ALTER TABLE t1 ADD COLUMN b JSON DEFAULT (JSON_OBJECT("key",i));
SELECT CONCAT(t.name, ".", c.name) as col_name,
              c.default_option, c.default_value_utf8
FROM mysql.tables AS t JOIN mysql.columns as c ON t.id = c.table_id
WHERE t.name = 't1' ORDER BY c.id;
INSERT INTO t1(i) VALUES (3);
INSERT INTO t1(i, b) VALUES (4, DEFAULT);
INSERT INTO t1(i, b) VALUES (5, JSON_OBJECT("key", 5));
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # Test ALTER table ADD column with DEFAULT then ALTER TABLE SET DEFAULT func
--echo #
CREATE TABLE t1 (i int);
INSERT INTO t1(i) VALUES (1),(2);
ALTER TABLE t1 ADD COLUMN b JSON;
ALTER TABLE t1 ALTER COLUMN b SET DEFAULT (JSON_OBJECT("key",i));
SELECT CONCAT(t.name, ".", c.name) as col_name,
              c.default_option, c.default_value_utf8
FROM mysql.tables AS t JOIN mysql.columns as c ON t.id = c.table_id
WHERE t.name = 't1' ORDER BY c.id;
INSERT INTO t1(i) VALUES (3);
INSERT INTO t1(i, b) VALUES (4, DEFAULT);
INSERT INTO t1(i, b) VALUES (5, JSON_OBJECT("key", 5));
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # Test CREATE TABLE with column without default then SET a DEFAULT.
--echo # ALSO TEST DROP DEFAULT
--echo #
CREATE TABLE t1 (i int, b JSON);
INSERT INTO t1(i) VALUES (1),(2);
ALTER TABLE t1 ALTER COLUMN b SET DEFAULT (JSON_OBJECT("key",i));
INSERT INTO t1(i) VALUES (3);
INSERT INTO t1(i, b) VALUES (4, DEFAULT);
INSERT INTO t1(i, b) VALUES (5, JSON_OBJECT("key", 5));
ALTER TABLE t1 ALTER COLUMN b DROP DEFAULT;
SELECT CONCAT(t.name, ".", c.name) as col_name,
              c.default_option, c.default_value_utf8
FROM mysql.tables AS t JOIN mysql.columns as c ON t.id = c.table_id
WHERE t.name = 't1' ORDER BY c.id;
INSERT INTO t1(i, b) VALUES (6, NULL);
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # Test SHOW COLUMNS for table with NOT NULL DEFAULT (expr).
--echo #
CREATE TABLE t1 (f1 INT NOT NULL DEFAULT (32));
SHOW COLUMNS FROM t1;
SELECT CONCAT(t.name, ".", c.name) as col_name,
              c.default_option, c.default_value_utf8
FROM mysql.tables AS t JOIN mysql.columns as c ON t.id = c.table_id
WHERE t.name = 't1' ORDER BY c.id;
DROP TABLE t1;

SET SESSION debug= '-d,skip_dd_table_access_check';
