################################################################################
# WL10905 - Support for INVISIBLE columns.                                     #
#           File includes tests common to all the storage engines.             #
################################################################################

--echo #------------------------------------------------------------------------
--echo # Test case to verify table creation with only INVISIBLE columns.
--echo #------------------------------------------------------------------------
--error ER_TABLE_MUST_HAVE_A_VISIBLE_COLUMN
CREATE TABLE t1 (f1 INT INVISIBLE, f2 INT INVISIBLE);

CREATE TABLE t1 (f1 INT);
--error ER_TABLE_MUST_HAVE_A_VISIBLE_COLUMN
ALTER TABLE t1 ALTER f1 SET INVISIBLE;
DROP TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify table with INVISIBLE columns.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT INVISIBLE VISIBLE);
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;
DROP TABLE t1;

CREATE TABLE t1 (f1 INT, f2 INT VISIBLE, f3 INT INVISIBLE,
                             f4 INT GENERATED ALWAYS AS
                                    ((`f1` + `f2`)) VIRTUAL INVISIBLE,
                             f5 INT GENERATED ALWAYS AS
                                    ((`f1` + `f2`)) STORED INVISIBLE);
--echo #SHOW CREATE TABLE lists INVISIBLE columns.
SHOW CREATE TABLE t1;
--echo #SHOW COLUMNS FROM lists invisible columns with INVISIBLE value in EXTRA column.
SHOW COLUMNS FROM t1;
--echo # For SE hidden columns INVISIBLE value is not listed in EXTRA column.
SHOW EXTENDED COLUMNS FROM t1;
DROP TABLE t1;

CREATE TEMPORARY TABLE temp (f1 INT, f2 INT VISIBLE, f3 INT INVISIBLE,
                             f4 INT GENERATED ALWAYS AS
                                    ((`f1` + `f2`)) VIRTUAL INVISIBLE,
                             f5 INT GENERATED ALWAYS AS
                                    ((`f1` + `f2`)) STORED INVISIBLE);
SHOW CREATE TABLE temp;
SHOW COLUMNS FROM temp;
SHOW EXTENDED COLUMNS FROM temp;
DROP TABLE temp;


--echo #------------------------------------------------------------------------
--echo # Test case to verify wildcard expansion in SELECT operation on table
--echo # with INVISIBLE columns.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT INVISIBLE);
INSERT INTO t1 (f1, f2) VALUES (1, 2);

# Only f1 column is listed.
SELECT * FROM t1;
# Both f1 and f2 columns are listed.
SELECT f1, f2 FROM t1;
# Only column f1 is expanded for wildcard. Only one f2 column is listed.
SELECT *, f2 FROM t1;
# Only column f1 is expanded for wildcard in sub-query.
SELECT t.f1 FROM (SELECT * FROM t1) AS t;
--error ER_BAD_FIELD_ERROR
SELECT t.f2 FROM (SELECT * FROM t1) AS t;

CREATE TABLE t2 (f3 INT, f2 INT INVISIBLE);
INSERT INTO t2 (f3, f2) VALUES (5, 2);
# Only f1 from t1 is listed and invisible column f2 from t2.
SELECT t1.*, t2.f2 FROM t1 JOIN t2;
DROP TABLE t1, t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INSERT and SELECT operations on table with
--echo # INVISIBLE columns.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT VISIBLE, f3 INT INVISIBLE);
# Only for VISIBLE columns values are inserted.
INSERT INTO t1 VALUES (10, 20);
# Only VISIBLE columns are listed.
SELECT * FROM t1;
# INVISIBLE column is listed when explicitly referenced.
SELECT f1, f2, f3 FROM t1;
DELETE FROM t1;

# Value is inserted for INVISIBLE columns too when referenced explicitly.
INSERT INTO t1(f1, f2, f3) VALUES (1, 2, 3);
SELECT * FROM t1;
SELECT f1, f2, f3 FROM t1;
DELETE FROM t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INVISIBLE columns with DEFAULT value.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 ALTER f3 SET DEFAULT 8;
SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES (10, 20);
SELECT * FROM t1;
SELECT f1, f2, f3 FROM t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify generated INVISIBLE columns.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN f4 INT AS (f1 + f2) INVISIBLE;
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;
SELECT * FROM t1;
SELECT f4 FROM t1;
DROP TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INVISIBLE column attribute for all type of columns.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (c0 INT,
  c1 BIT(7) INVISIBLE,
  c2 BOOLEAN INVISIBLE,
  c3 TINYINT INVISIBLE,
  c4 SMALLINT INVISIBLE,
  c5 MEDIUMINT INVISIBLE,
  c6 INT INVISIBLE,
  c7 BIGINT INVISIBLE,
  c8 DECIMAL(6,2) INVISIBLE,
  c9 FLOAT INVISIBLE,
  c10 DOUBLE INVISIBLE,
  c11 CHAR(1) INVISIBLE,
  c12 VARCHAR(1) INVISIBLE,
  c13 BINARY(1) INVISIBLE,
  c14 VARBINARY(1) INVISIBLE,
  c15 TINYBLOB INVISIBLE,
  c16 TINYTEXT INVISIBLE,
  c17 BLOB INVISIBLE,
  c18 TEXT INVISIBLE,
  c19 MEDIUMBLOB INVISIBLE,
  c20 MEDIUMTEXT INVISIBLE,
  c21 LONGBLOB INVISIBLE,
  c22 LONGTEXT INVISIBLE,
  c23 DATE INVISIBLE,
  c24 DATETIME INVISIBLE,
  c25 TIMESTAMP INVISIBLE,
  c26 TIME INVISIBLE,
  c27 YEAR INVISIBLE,
  c28 JSON INVISIBLE,
  c29 ENUM('a', 'b') INVISIBLE,
  c30 SET('a', 'b') INVISIBLE,
  c31 POINT INVISIBLE,
  c32 LINESTRING INVISIBLE,
  c33 MULTILINESTRING INVISIBLE,
  c34 POLYGON INVISIBLE,
  c35 MULTIPOLYGON INVISIBLE
);
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;

INSERT INTO t1 VALUES (10);
SELECT * FROM t1;
DROP TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INVISIBLE column update.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT INVISIBLE DEFAULT 8);
INSERT INTO t1(f1, f2) VALUES  (10, 10), (20, 8);
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;

SELECT f1, f2 FROM t1 ORDER BY f1;
DELETE FROM t1 WHERE f2 = 10;
UPDATE t1 SET f2 = 20;
SELECT f1, f2 FROM t1;
DELETE FROM t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify Index creation on INVISIBLE column.
--echo #------------------------------------------------------------------------
INSERT INTO t1 VALUES  (10);
ALTER TABLE t1 ADD INDEX idx(f2);
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify Unique Index creation on INVISIBLE column.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 DROP INDEX idx, ADD UNIQUE INDEX (f2);
SHOW CREATE TABLE t1;
--error ER_DUP_ENTRY
INSERT INTO t1 VALUES  (10);
INSERT INTO t1(f1, f2) VALUES  (10, 10);
SELECT f1, f2 FROM t1 ORDER BY f2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify Primary Key creation on INVISIBLE column.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 DROP INDEX f2, ADD PRIMARY KEY (f2);
SHOW CREATE TABLE t1;
--error ER_DUP_ENTRY
INSERT INTO t1 VALUES  (10);
INSERT INTO t1(f1, f2) VALUES  (10, 20);
SELECT f1, f2 FROM t1 ORDER BY f2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INVISIBLE auto_increment column.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 MODIFY f2 INT INVISIBLE AUTO_INCREMENT;
SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES  (10);
SELECT f1, f2 FROM t1 ORDER BY f2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify Check Constraint on INVISIBLE column.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 MODIFY f2 INT INVISIBLE, ADD CHECK(f2 < 20000);
SHOW CREATE TABLE t1;
--error ER_CHECK_CONSTRAINT_VIOLATED
INSERT INTO t1(f1, f2) VALUES  (10, 20000);
SELECT f1, f2 FROM t1 ORDER BY f2;
DROP TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify column visibility alter using ALTER clause of
--echo # ALTER TABLE statement.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT);
INSERT INTO t1 VALUES (10, 90);
SHOW CREATE TABLE t1;
SELECT * FROM t1;

ALTER TABLE t1 ALTER COLUMN f1 SET INVISIBLE;
SHOW CREATE TABLE t1;
SELECT * FROM t1;
SELECT f1, f2 FROM t1;
--error ER_TABLE_MUST_HAVE_A_VISIBLE_COLUMN
ALTER TABLE t1 ALTER COLUMN f2 SET INVISIBLE;

ALTER TABLE t1 ALTER COLUMN f1 SET VISIBLE;
SHOW CREATE TABLE t1;
SELECT * FROM t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify column visibility alter using CHANGE clause of
--echo # ALTER TABLE statement.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 CHANGE f1 f1 INT INVISIBLE;
SHOW CREATE TABLE t1;
SELECT * FROM t1;
SELECT f1, f2 FROM t1;
--error ER_TABLE_MUST_HAVE_A_VISIBLE_COLUMN
ALTER TABLE t1 CHANGE f2 f2 INT INVISIBLE;

ALTER TABLE t1 CHANGE f1 f1 INT VISIBLE;
SHOW CREATE TABLE t1;
SELECT * FROM t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify column visibility alter using MODIFY clause of
--echo # ALTER TABLE statement.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 MODIFY f1 INT INVISIBLE;
SHOW CREATE TABLE t1;
SELECT * FROM t1;
SELECT f1, f2 FROM t1;
--error ER_TABLE_MUST_HAVE_A_VISIBLE_COLUMN
ALTER TABLE t1 MODIFY f2 INT INVISIBLE;

ALTER TABLE t1 MODIFY f1 INT VISIBLE;
SHOW CREATE TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify LOAD DATA.
--echo #------------------------------------------------------------------------
CREATE TABLE t1(f1 INT INVISIBLE, f2 INT);
CREATE TABLE t2(f1 INT, f2 INT);
SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;

INSERT INTO t1(f1, f2) VALUES (10, 20), (20, 30);
INSERT INTO t2 VALUES (10, 20), (20, 30);

# without column list
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval SELECT * FROM t1 INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/test1.sql';
DELETE FROM t1;
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/tmp/test1.sql' INTO TABLE t1;
SELECT * FROM t1 ORDER BY f2;
SELECT f1, f2 FROM t1 ORDER BY f2;
DELETE FROM t1;
# with INVISIBLE column in column list.
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/tmp/test1.sql' INTO TABLE t1 (f1) SET f2 = 10;
SELECT * FROM t1 ORDER BY f2;
SELECT f1, f2 FROM t1 ORDER BY f1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify LOAD DATA from fixed length row.
--echo #------------------------------------------------------------------------
# without column list
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval SELECT * FROM t1 INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/test2.sql' FIELDS TERMINATED BY '' ENCLOSED BY '';
DELETE FROM t1;
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/tmp/test2.sql' INTO TABLE t1 FIELDS TERMINATED BY '' ENCLOSED BY '';
SELECT * FROM t1;
SELECT f1, f2 FROM t1;
DELETE FROM t1;
# with INVISIBLE column in column list.
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/tmp/test2.sql' INTO TABLE t1 FIELDS TERMINATED BY '' ENCLOSED BY '' (f1) SET f2 = f1 + 10;
SELECT * FROM t1;
SELECT f1, f2 FROM t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify LOAD DATA in XML format.
--echo #------------------------------------------------------------------------
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--exec $MYSQL_DUMP --xml test t1 > "$MYSQLTEST_VARDIR/tmp/tmp1.xml" 2>&1
DELETE FROM t1;
# With INVISIBLE column in column list.
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval LOAD XML INFILE "$MYSQLTEST_VARDIR/tmp/tmp1.xml" INTO TABLE t1 (f1, f2);
SELECT * FROM t1;
SELECT f1, f2 FROM t1;
ALTER TABLE t1 DROP COLUMN f1;
# without column list
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--exec $MYSQL_DUMP --xml test t1 > "$MYSQLTEST_VARDIR/tmp/tmp1.xml" 2>&1
DELETE FROM t1;
ALTER TABLE t1 ADD COLUMN f1 INT INVISIBLE;
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval LOAD XML INFILE "$MYSQLTEST_VARDIR/tmp/tmp1.xml" INTO TABLE t1;
SELECT * FROM t1;
SELECT f1, f2 FROM t1;

DROP TABLE t1, t2;
remove_file $MYSQLTEST_VARDIR/tmp/test1.sql;
remove_file $MYSQLTEST_VARDIR/tmp/test2.sql;
remove_file $MYSQLTEST_VARDIR/tmp/tmp1.xml;


--echo #------------------------------------------------------------------------
--echo # Test case to verify view on INVISIBLE column.
--echo #------------------------------------------------------------------------
CREATE TABLE t1(f1 INT INVISIBLE, f2 INT);
INSERT INTO t1(f1, f2) VALUES (10, 20), (30, 40);

--echo # View is created with only f2.
CREATE VIEW v1 AS SELECT * FROM t1;
SHOW CREATE VIEW v1;
SHOW COLUMNS FROM v1;
SELECT * FROM v1 ORDER BY f2;

--echo # View is created with f1 and f2;
CREATE VIEW v2 AS SELECT f1, f2 FROM t1;
SHOW CREATE VIEW v2;
--echo # Both columns are listed as VISIBLE.
SHOW COLUMNS FROM v2;
SELECT * FROM v2 ORDER BY f2;

--echo # View is created with only f1;
CREATE VIEW v3 AS SELECT f1 FROM t1;
SHOW CREATE VIEW v3;
--echo # Column f1 is listed as VISIBLE.
SHOW COLUMNS FROM v3;
SELECT * FROM v3 ORDER BY f1;
DROP VIEW v1, v2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INFORMATION_SCHEMA.COLUMNS result set.
--echo #------------------------------------------------------------------------
SELECT TABLE_NAME, COLUMN_NAME, EXTRA FROM INFORMATION_SCHEMA.COLUMNS
WHERE TABLE_SCHEMA='test' ORDER BY TABLE_NAME, COLUMN_NAME;
DROP VIEW v3;


--echo #------------------------------------------------------------------------
--echo # Test case to verify REPLACE operation with INVISIBLE column.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 ADD PRIMARY KEY (f1);
SHOW CREATE TABLE t1;
SELECT f1, f2 FROM t1 ORDER BY f2;
REPLACE INTO t1(f1, f2) VALUE (10, 88);
SELECT f1, f2 FROM t1 ORDER BY f2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INSERT ON DUPLICATE KEY UPDATE operation with
--echo # INVISIBLE column.
--echo #------------------------------------------------------------------------
INSERT INTO t1(f1, f2) VALUES (30, 44) ON DUPLICATE KEY UPDATE f2=88;
SELECT f1, f2 FROM t1 ORDER BY f1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INVISIBLE columns with CREATE TABLE LIKE
--echo #------------------------------------------------------------------------
CREATE TABLE t2 LIKE t1;
SHOW CREATE TABLE t2;
SHOW COLUMNS FROM t2;
DROP TABLE t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify INVISIBLE columns with CREATE TABLE ... SELECT
--echo #------------------------------------------------------------------------
CREATE TABLE t2 SELECT * FROM t1;
--echo # Table is created with only f2 column.
SHOW CREATE TABLE t2;
SHOW COLUMNS FROM t2;
SELECT * FROM t2;
DROP TABLE t2;

CREATE TABLE t2 SELECT f1, f2 FROM t1;
--echo # Both f1 and f2 are created as VISIBLE columns.
SHOW CREATE TABLE t2;
SHOW COLUMNS FROM t2;
SELECT * FROM t2 ORDER BY f1;
DROP TABLE t2;

CREATE TABLE t2(f1 INT INVISIBLE) SELECT f1, f2 FROM t1;
--echo # f1 is created as INVISIBLE columns and f2 as VISIBLE columns.
SHOW CREATE TABLE t2;
SHOW COLUMNS FROM t2;
SELECT * FROM t2;
DROP TABLE t2;

CREATE TEMPORARY TABLE t2 SELECT * FROM t1;
--echo # Table is created with only f2 column.
SHOW CREATE TABLE t2;
SHOW COLUMNS FROM t2;
SELECT * FROM t2;
DROP TABLE t2;

CREATE TEMPORARY TABLE t2 SELECT f1, f2 FROM t1;
--echo # Both f1 and f2 are created as VISIBLE columns.
SHOW CREATE TABLE t2;
SHOW COLUMNS FROM t2;
SELECT * FROM t2 ORDER BY f1;
DROP TABLE t2;

CREATE TEMPORARY TABLE t2(f1 INT INVISIBLE) SELECT f1, f2 FROM t1;
--echo # f1 is created as INVISIBLE columns and f2 as VISIBLE columns.
SHOW CREATE TABLE t2;
SHOW COLUMNS FROM t2;
SELECT * FROM t2;
DROP TABLE t2;
DROP TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify table and column privileges on INVISIBLE columns.
--echo #------------------------------------------------------------------------
--enable_connect_log
CREATE USER user1@localhost;
CREATE DATABASE db1;
USE db1;
CREATE TABLE t1(f1 INT DEFAULT 10, f2 INT);
INSERT INTO t1(f2) VALUES(1);

GRANT SELECT(f2) ON db1.t1 TO user1@localhost;

--echo # user1 has SELECT privilege on only column f2;
connect (con1, localhost, user1, , db1);
--error ER_COLUMNACCESS_DENIED_ERROR
SELECT * FROM t1;

--echo # Make t1.f1 invisible.
connection default;
ALTER TABLE t1 ALTER COLUMN f1 SET INVISIBLE;

--echo # 'SELECT *' works as only column f2 is listed.
connection con1;
SELECT * FROM t1;
--error ER_COLUMNACCESS_DENIED_ERROR
SELECT f1 FROM t1;

connection default;
REVOKE ALL ON db1.t1 FROM user1@localhost;

--echo # user1 does not have any privilges on t1 table.
connection con1;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO t1 VALUES(2);
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO t1(f1) VALUES (11);

--echo # Table privilege INSERT is granted to user1.
connection default;
GRANT INSERT ON db1.t1 TO user1@localhost;

connection con1;
INSERT INTO t1 VALUES(2);
INSERT INTO t1(f1) VALUES (11);

--echo # ALL privileges are REVOKED from user1.
connection default;
REVOKE ALL ON db1.t1 FROM user1@localhost;

connection con1;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO t1 VALUES(3);
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO t1(f1) VALUES (12);

--echo # INSERT on t1.f1 column and SELECT on t1.f1, t1.f2 columns are granted to user1
connection default;
GRANT INSERT(f1), SELECT(f1, f2) ON db1.t1 TO user1@localhost;

connection con1;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO t1 VALUES(3);
INSERT INTO t1(f1) VALUES (12);

connection default;
disconnect con1;
USE test;
DROP DATABASE db1;
DROP USER user1@localhost;
--disable_connect_log


--echo #------------------------------------------------------------------------
--echo # Test case to verify natural join with invisible columns.
--echo #------------------------------------------------------------------------
CREATE TABLE t1(a INT, b INT INVISIBLE);
CREATE TABLE t2(c INT, b INT INVISIBLE);

INSERT INTO t1(b) VALUES (1), (2), (3);
INSERT INTO t2(b) VALUES (1), (2), (3);

SELECT * FROM t1 JOIN t2;
SELECT * FROM t1 JOIN t2 WHERE (t1.b = t2.b);
SELECT * FROM t1 JOIN t2 ON (t1.b = t2.b);
SELECT * FROM t1 JOIN t2 USING(b) ORDER BY (b);

DROP TABLE t1, t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify my_row_id name usage for invisible columns.
--echo #------------------------------------------------------------------------
CREATE TABLE t1(a INT, my_row_id INT);
# Following statement should not fail.
CREATE TABLE t2(a INT, my_row_id INT INVISIBLE);
DROP TABLE t1, t2;
