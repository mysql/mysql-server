
--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

CREATE TABLE t1 (a INT, b INT, c INT, UNIQUE (A), UNIQUE(B));
INSERT t1 VALUES (1,2,10), (3,4,20);
INSERT t1 VALUES (5,6,30) ON DUPLICATE KEY UPDATE c=c+100;
SELECT * FROM t1;
INSERT t1 VALUES (5,7,40) ON DUPLICATE KEY UPDATE c=c+100;
SELECT * FROM t1;
INSERT t1 VALUES (8,4,50) ON DUPLICATE KEY UPDATE c=c+1000;
SELECT * FROM t1;
INSERT t1 VALUES (1,4,60) ON DUPLICATE KEY UPDATE c=c+10000;
SELECT * FROM t1;
-- error ER_DUP_ENTRY
INSERT t1 VALUES (1,9,70) ON DUPLICATE KEY UPDATE c=c+100000, b=4;
SELECT * FROM t1;
TRUNCATE TABLE t1;
INSERT t1 VALUES (1,2,10), (3,4,20);
INSERT t1 VALUES (5,6,30), (7,4,40), (8,9,60) ON DUPLICATE KEY UPDATE c=c+100;
SELECT * FROM t1;
INSERT t1 SET a=5 ON DUPLICATE KEY UPDATE b=0;
SELECT * FROM t1;
INSERT t1 VALUES (2,1,11), (7,4,40) ON DUPLICATE KEY UPDATE c=c+VALUES(a);
SELECT *, VALUES(a) FROM t1;
analyze table t1;
--replace_regex /[56]/#/
explain SELECT *, VALUES(a) FROM t1;
explain select * from t1 where values(a);
DROP TABLE t1;

#
# test for Bug #2709 "Affected Rows for ON DUPL.KEY undocumented, 
#                                                 perhaps illogical"
#
create table t1(a int primary key, b int);
insert into t1 values(1,1),(2,2),(3,3),(4,4),(5,5);
select * from t1;

--enable_info
insert into t1 values(4,14),(5,15),(6,16),(7,17),(8,18)
 on duplicate key update b=b+10;
--disable_info

select * from t1;

enable_info;
replace into t1 values(5,25),(6,26),(7,27),(8,28),(9,29);
disable_info;

select * from t1;
drop table t1;

# WorkLog #2274 - enable INSERT .. SELECT .. UPDATE syntax
# Same tests as beginning of this test except that insert source
# is a result from a select statement
#
CREATE TABLE t1 (a INT, b INT, c INT, UNIQUE (A), UNIQUE(B));
INSERT t1 VALUES (1,2,10), (3,4,20);
INSERT t1 SELECT 5,6,30 FROM DUAL ON DUPLICATE KEY UPDATE c=c+100;
SELECT * FROM t1;
INSERT t1 SELECT 5,7,40 FROM DUAL ON DUPLICATE KEY UPDATE c=c+100;
SELECT * FROM t1;
INSERT t1 SELECT 8,4,50 FROM DUAL ON DUPLICATE KEY UPDATE c=c+1000;
SELECT * FROM t1;
INSERT t1 SELECT 1,4,60 FROM DUAL ON DUPLICATE KEY UPDATE c=c+10000;
SELECT * FROM t1;
-- error ER_DUP_ENTRY
INSERT t1 SELECT 1,9,70 FROM DUAL ON DUPLICATE KEY UPDATE c=c+100000, b=4;
SELECT * FROM t1;
TRUNCATE TABLE t1;
INSERT t1 VALUES (1,2,10), (3,4,20);
CREATE TABLE t2 (a INT, b INT, c INT, d INT);
# column names deliberately clash with columns in t1 (Bug#8147)
INSERT t2 VALUES (5,6,30,1), (7,4,40,1), (8,9,60,1);
INSERT t2 VALUES (2,1,11,2), (7,4,40,2);
INSERT t1 SELECT a,b,c FROM t2 WHERE d=1 ON DUPLICATE KEY UPDATE c=t1.c+100;
SELECT * FROM t1;
INSERT t1 SET a=5 ON DUPLICATE KEY UPDATE b=0;
SELECT * FROM t1;
--error ER_NON_UNIQ_ERROR
INSERT t1 SELECT a,b,c FROM t2 WHERE d=2 ON DUPLICATE KEY UPDATE c=c+VALUES(a);
INSERT t1 SELECT a,b,c FROM t2 WHERE d=2 ON DUPLICATE KEY UPDATE c=t1.c+VALUES(t1.a);
SELECT *, VALUES(a) FROM t1;
DROP TABLE t1;
DROP TABLE t2;

#
# Bug#21555: incorrect behavior with INSERT ... ON DUPL KEY UPDATE and VALUES
#


# End of 4.1 tests
CREATE TABLE t1
(
  a   BIGINT UNSIGNED,
  b   BIGINT UNSIGNED,
  PRIMARY KEY (a)
);

INSERT INTO t1 VALUES (45, 1) ON DUPLICATE KEY UPDATE b =
  IF(VALUES(b) > t1.b, VALUES(b), t1.b);
SELECT * FROM t1;
INSERT INTO t1 VALUES (45, 2) ON DUPLICATE KEY UPDATE b =
  IF(VALUES(b) > t1.b, VALUES(b), t1.b);
SELECT * FROM t1;
INSERT INTO t1 VALUES (45, 1) ON DUPLICATE KEY UPDATE b = 
  IF(VALUES(b) > t1.b, VALUES(b), t1.b);
SELECT * FROM t1;

DROP TABLE t1;

#
# Bug#25831: Deficiencies in INSERT ... SELECT ... field name resolving.
#
CREATE TABLE t1 (i INT PRIMARY KEY, j INT);
--error ER_BAD_FIELD_ERROR
INSERT INTO t1 SELECT 1, j;
DROP TABLE t1;

CREATE TABLE t1 (i INT PRIMARY KEY, j INT);
CREATE TABLE t2 (a INT, b INT);
CREATE TABLE t3 (a INT, c INT);
INSERT INTO t1 SELECT 1, a FROM t2 NATURAL JOIN t3 
  ON DUPLICATE KEY UPDATE j= a;
DROP TABLE t1,t2,t3;

CREATE TABLE t1 (i INT PRIMARY KEY, j INT);
CREATE TABLE t2 (a INT);
INSERT INTO t1 VALUES (1, 1);
INSERT INTO t2 VALUES (1), (3);
--error ER_BAD_FIELD_ERROR
INSERT INTO t1 SELECT 1, COUNT(*) FROM t2 ON DUPLICATE KEY UPDATE j= a;
DROP TABLE t1,t2;

#
# Bug #26261: Missing default value isn't noticed in 
#   insert ... on duplicate key update
#
SET SQL_MODE = 'TRADITIONAL';

CREATE TABLE t1 (a INT PRIMARY KEY, b INT NOT NULL);

--error ER_NO_DEFAULT_FOR_FIELD
INSERT INTO t1 (a) VALUES (1);

--error ER_NO_DEFAULT_FOR_FIELD
INSERT INTO t1 (a) VALUES (1) ON DUPLICATE KEY UPDATE a = b;

--error ER_NO_DEFAULT_FOR_FIELD
INSERT INTO t1 (a) VALUES (1) ON DUPLICATE KEY UPDATE b = b;

SELECT * FROM t1;

DROP TABLE t1;

#
# Bug#27033: 0 as LAST_INSERT_ID() after INSERT .. ON DUPLICATE if rows were
#            touched but not actually changed.
#
CREATE TABLE t1 (f1 INT AUTO_INCREMENT PRIMARY KEY,
                 f2 VARCHAR(5) NOT NULL UNIQUE);
INSERT t1 (f2) VALUES ('test') ON DUPLICATE KEY UPDATE f1 = LAST_INSERT_ID(f1);
SELECT LAST_INSERT_ID();
INSERT t1 (f2) VALUES ('test') ON DUPLICATE KEY UPDATE f1 = LAST_INSERT_ID(f1);
SELECT LAST_INSERT_ID();
DROP TABLE t1;

#
# Bug#23233: 0 as LAST_INSERT_ID() after INSERT .. ON DUPLICATE in the
#            NO_AUTO_VALUE_ON_ZERO mode.
#
SET SQL_MODE='NO_AUTO_VALUE_ON_ZERO';
CREATE TABLE `t1` (
  `id` int(11) PRIMARY KEY auto_increment,
  `f1` varchar(10) NOT NULL UNIQUE,
  tim1 timestamp default '2003-01-01 00:00:00' on update current_timestamp
);
INSERT INTO t1 (f1) VALUES ("test1");
SELECT id, f1 FROM t1;
REPLACE INTO t1 VALUES (0,"test1",null);
SELECT id, f1 FROM t1;
DROP TABLE t1;
SET SQL_MODE='';

#
# Bug#27954: multi-row INSERT ... ON DUPLICATE with duplicated
# row at the first place into table with AUTO_INCREMENT and
# additional UNIQUE key.
#
CREATE TABLE t1 (
  id INT AUTO_INCREMENT PRIMARY KEY,
  c1 CHAR(1) UNIQUE KEY,
  cnt INT DEFAULT 1
);
INSERT INTO t1 (c1) VALUES ('A'), ('B'), ('C');
SELECT * FROM t1;
INSERT  INTO t1 (c1) VALUES ('A'), ('X'), ('Y'), ('Z')
  ON DUPLICATE KEY UPDATE cnt=cnt+1;
SELECT * FROM t1;
DROP TABLE t1;

#
# Bug#28000: INSERT IGNORE ... SELECT ... ON DUPLICATE
# with erroneous UPDATE: NOT NULL field with NULL value.
#
CREATE TABLE t1 (
  id INT AUTO_INCREMENT PRIMARY KEY,
  c1 INT NOT NULL,
  cnt INT DEFAULT 1
);
INSERT INTO t1 (id,c1) VALUES (1,10);
SELECT * FROM t1;
CREATE TABLE t2 (id INT, c1 INT);
INSERT INTO t2 VALUES (1,NULL), (2,2);
--error ER_BAD_NULL_ERROR
INSERT INTO t1 (id,c1) SELECT 1,NULL
  ON DUPLICATE KEY UPDATE c1=NULL;
SELECT * FROM t1;
INSERT IGNORE INTO t1 (id,c1) SELECT 1,NULL
  ON DUPLICATE KEY UPDATE c1=NULL, cnt=cnt+1;
SELECT * FROM t1;
INSERT IGNORE INTO t1 (id,c1) SELECT * FROM t2
  ON DUPLICATE KEY UPDATE c1=NULL, cnt=cnt+1;
SELECT * FROM t1;

DROP TABLE t1;
DROP TABLE t2;

#
# Bug#28904: INSERT .. ON DUPLICATE was silently updating rows when it
#            shouldn't.
#
create table t1(f1 int primary key,
 f2 timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP);
insert into t1(f1) values(1);
--replace_column 1 #
select @stamp1:=f2 from t1;
--sleep 2
insert into t1(f1) values(1) on duplicate key update f1=1;
--replace_column 1 #
select @stamp2:=f2 from t1;
select if( @stamp1 = @stamp2, "correct", "wrong");
drop table t1;

--echo # Bug 21774967: MYSQL ACCEPTS NON-ASCII IN ASCII COLUMNS
CREATE TABLE t1(
  a CHAR(20) CHARACTER SET ascii,
  b VARCHAR(20) CHARACTER SET ascii,
  c TEXT(20) CHARACTER SET ascii
);
CREATE TABLE t2(
  a CHAR(20) CHARACTER SET ascii COLLATE ascii_general_ci,
  b VARCHAR(20) CHARACTER SET ascii COLLATE ascii_general_ci,
  c TEXT(20) CHARACTER SET ascii COLLATE ascii_general_ci
);
CREATE TABLE t3(
  a CHAR(20) CHARACTER SET ascii COLLATE ascii_bin,
  b VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin,
  c TEXT(20) CHARACTER SET ascii COLLATE ascii_bin
);

SET SQL_MODE="STRICT_TRANS_TABLES";
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 values(x'8142', x'8142', x'8142');
INSERT INTO t1 values(x'6162', x'6162', x'6162');
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
UPDATE t1 SET a=x'8243' where a=x'6162';
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t2 values(x'8142', x'8142', x'8142');
INSERT INTO t2 values(x'6162', x'6162', x'6162');
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
UPDATE t2 SET a=x'8243' where a=x'6162';
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t3 values(x'8142', x'8142', x'8142');
INSERT INTO t3 values(x'6162', x'6162', x'6162');
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
UPDATE t3 SET a=x'8243' where a=x'6162';
SET SQL_MODE="";
INSERT INTO t1 values(x'8142', x'8142', x'8142');
INSERT INTO t1 values(x'6162', x'6162', x'6162');
# Different row number with the hypergraph optimizer.
--replace_regex /at row 2$/at row 3/
UPDATE t1 SET a=x'8243' where a=x'6162';
INSERT INTO t2 values(x'8142', x'8142', x'8142');
INSERT INTO t2 values(x'6162', x'6162', x'6162');
# Different row number with the hypergraph optimizer.
--replace_regex /at row 2$/at row 3/
UPDATE t2 SET a=x'8243' where a=x'6162';
INSERT INTO t3 values(x'8142', x'8142', x'8142');
INSERT INTO t3 values(x'6162', x'6162', x'6162');
# Different row number with the hypergraph optimizer.
--replace_regex /at row 2$/at row 3/
UPDATE t3 SET a=x'8243' where a=x'6162';

CREATE VIEW v1 AS SELECT * FROM t1;
CREATE VIEW v2 AS SELECT * FROM t2;
CREATE VIEW v3 AS SELECT * FROM t3;

SET SQL_MODE="STRICT_TRANS_TABLES";
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO v1 values(x'8142', x'8142', x'8142');
INSERT INTO v1 values(x'6162', x'6162', x'6162');
# Different row number with the hypergraph optimizer.
--replace_regex /at row 1$/at row 4/
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
UPDATE v1 SET a=x'8243' where a=x'6162';
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO v2 values(x'8142', x'8142', x'8142');
INSERT INTO v2 values(x'6162', x'6162', x'6162');
# Different row number with the hypergraph optimizer.
--replace_regex /at row 1$/at row 4/
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
UPDATE v2 SET a=x'8243' where a=x'6162';
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO v3 values(x'8142', x'8142', x'8142');
INSERT INTO v3 values(x'6162', x'6162', x'6162');
# Different row number with the hypergraph optimizer.
--replace_regex /at row 1$/at row 4/
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
UPDATE v3 SET a=x'8243' where a=x'6162';
SET SQL_MODE="";
INSERT INTO v1 values(x'8142', x'8142', x'8142');
INSERT INTO v1 values(x'6162', x'6162', x'6162');
# Different row numbers with the hypergraph optimizer.
--replace_regex /at row 1$/at row 4/ /at row 2$/at row 6/
UPDATE v1 SET a=x'8243' where a=x'6162';
INSERT INTO v2 values(x'8142', x'8142', x'8142');
INSERT INTO v2 values(x'6162', x'6162', x'6162');
# Different row numbers with the hypergraph optimizer.
--replace_regex /at row 1$/at row 4/ /at row 2$/at row 6/
UPDATE v2 SET a=x'8243' where a=x'6162';
INSERT INTO v3 values(x'8142', x'8142', x'8142');
INSERT INTO v3 values(x'6162', x'6162', x'6162');
# Different row numbers with the hypergraph optimizer.
--replace_regex /at row 1$/at row 4/ /at row 2$/at row 6/
UPDATE v3 SET a=x'8243' where a=x'6162';

DROP VIEW v1;
DROP VIEW v2;
DROP VIEW v3;

DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;

CREATE TABLE t_latin1(
  a CHAR(20) CHARACTER SET latin1,
  b VARCHAR(20) CHARACTER SET latin1,
  c TEXT(20) CHARACTER SET latin1
);
CREATE TABLE t_gb2312(
  a CHAR(20) CHARACTER SET gb2312,
  b VARCHAR(20) CHARACTER SET gb2312,
  c TEXT(20) CHARACTER SET gb2312
);
CREATE TABLE t_utf8(
  a CHAR(20) CHARACTER SET utf8mb3,
  b VARCHAR(20) CHARACTER SET utf8mb3,
  c TEXT(20) CHARACTER SET utf8mb3
);
SET SQL_MODE="STRICT_TRANS_TABLES";
INSERT INTO t_latin1 values(x'f242', x'f242', x'f242');
UPDATE t_latin1 SET a=x'f343' where a=x'f242';
INSERT INTO t_gb2312 values(x'e5ac', x'e5ac', x'e5ac');
UPDATE t_gb2312 SET a=x'e6af' where a=x'e5ac';

INSERT INTO t_utf8 values(x'e4b8ad', x'e4b8ad', x'e4b8ad');
--disable_abort_on_error
INSERT INTO t_utf8 values(x'f4b8ad', x'f4b8ad', x'f4b8ad');
--enable_abort_on_error
UPDATE t_utf8 SET a=x'e69687' where a=x'e4b8ad';
--disable_abort_on_error
UPDATE t_utf8 SET a=x'f69687' where a=x'e69687';
--enable_abort_on_error
SET SQL_MODE="";
INSERT INTO t_latin1 values(x'f242', x'f242', x'f242');
UPDATE t_latin1 SET a=x'f343' where a=x'f242';
INSERT INTO t_gb2312 values(x'e5ac', x'e5ac', x'e5ac');
UPDATE t_gb2312 SET a=x'e6af' where a=x'e5ac';

INSERT INTO t_utf8 values(x'e4b8ad', x'e4b8ad', x'e4b8ad');
--disable_abort_on_error
INSERT INTO t_utf8 values(x'f4b8ad', x'f4b8ad', x'f4b8ad');
--enable_abort_on_error
UPDATE t_utf8 SET a=x'e69687' where a=x'e4b8ad';
--disable_abort_on_error
UPDATE t_utf8 SET a=x'f69687' where a=x'e69687';
--enable_abort_on_error

CREATE VIEW v_latin1 AS SELECT * FROM t_latin1;
CREATE VIEW v_gb2312 AS SELECT * FROM t_gb2312;
CREATE VIEW v_utf8 AS SELECT * FROM t_utf8;

SET SQL_MODE="STRICT_TRANS_TABLES";
INSERT INTO v_latin1 values(x'f242', x'f242', x'f242');
UPDATE v_latin1 SET a=x'f343' where a=x'f242';
INSERT INTO v_gb2312 values(x'e5ac', x'e5ac', x'e5ac');
UPDATE v_gb2312 SET a=x'e6af' where a=x'e5ac';

INSERT INTO v_utf8 values(x'e4b8ad', x'e4b8ad', x'e4b8ad');
--disable_abort_on_error
INSERT INTO v_utf8 values(x'f4b8ad', x'f4b8ad', x'f4b8ad');
--enable_abort_on_error
UPDATE v_utf8 SET a=x'e69687' where a=x'e4b8ad';
--disable_abort_on_error
# Different row number with the hypergraph optimizer.
--replace_regex /at row 1$/at row 4/
UPDATE v_utf8 SET a=x'f69687' where a=x'e69687';
--enable_abort_on_error
SET SQL_MODE="";
INSERT INTO v_latin1 values(x'f242', x'f242', x'f242');
UPDATE v_latin1 SET a=x'f343' where a=x'f242';
INSERT INTO v_gb2312 values(x'e5ac', x'e5ac', x'e5ac');
UPDATE v_gb2312 SET a=x'e6af' where a=x'e5ac';

INSERT INTO v_utf8 values(x'e4b8ad', x'e4b8ad', x'e4b8ad');
--disable_abort_on_error
INSERT INTO v_utf8 values(x'f4b8ad', x'f4b8ad', x'f4b8ad');
--enable_abort_on_error
UPDATE v_utf8 SET a=x'e69687' where a=x'e4b8ad';
--disable_abort_on_error
# Different row numbers with the hypergraph optimizer.
--replace_regex /at row 1$/at row 4/ /at row 2$/at row 5/
UPDATE v_utf8 SET a=x'f69687' where a=x'e69687';
--enable_abort_on_error

DROP VIEW v_latin1;
DROP VIEW v_gb2312;
DROP VIEW v_utf8;

DROP TABLE t_latin1;
DROP TABLE t_gb2312;
DROP TABLE t_utf8;
SET SQL_MODE=DEFAULT;

--echo # WL#5094: Refactor DML statements
--echo # Semantic changes in INSERT ... ON DUPLICATE KEY

CREATE TABLE t0 (k INTEGER PRIMARY KEY);
CREATE TABLE t1(a INTEGER);
CREATE TABLE t2(a INTEGER);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1), (3);
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

--echo # Allowed: Reference column from a single table
INSERT INTO t0
SELECT a FROM t1
ON DUPLICATE KEY UPDATE k= a + t1.a + 10;

SELECT * FROM t0;

DELETE FROM t0;
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

--echo # Allowed: Reference column from a join over multiple tables
INSERT INTO t0
SELECT t1.a FROM t1 JOIN t2 ON t1.a=t2.a
ON DUPLICATE KEY UPDATE k= t1.a + t2.a + 10;

SELECT * FROM t0;

DELETE FROM t0;
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

INSERT INTO t0
SELECT a FROM t1 JOIN t2 USING (a)
ON DUPLICATE KEY UPDATE k= t1.a + t2.a + 10;

SELECT * FROM t0;

DELETE FROM t0;
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

INSERT INTO t0
SELECT a FROM t1 LEFT JOIN t2 USING (a)
ON DUPLICATE KEY UPDATE k= a + 10;

SELECT * FROM t0;

DELETE FROM t0;
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

INSERT INTO t0
SELECT DISTINCT a FROM t1
ON DUPLICATE KEY UPDATE k= a + 10;

SELECT * FROM t0;

DELETE FROM t0;
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

--echo # Allowed: Wrap a distinct query in a derived table

INSERT INTO t0
SELECT a FROM (SELECT DISTINCT a FROM t1) AS dt
ON DUPLICATE KEY UPDATE k= a + 10;

SELECT * FROM t0;

DELETE FROM t0;
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

--echo # Not allowed: Reference column from an explicitly grouped query

--error ER_BAD_FIELD_ERROR
INSERT INTO t0
SELECT a FROM t1 GROUP BY a
ON DUPLICATE KEY UPDATE k= a + 10;

--echo # Not allowed: Reference column from an implicitly grouped query

--error ER_BAD_FIELD_ERROR
INSERT INTO t0
SELECT SUM(a) FROM t1
ON DUPLICATE KEY UPDATE k= a + 10;

--echo # Allowed: Wrap a grouped query in a derived table

INSERT INTO t0
SELECT a FROM (SELECT a FROM t1 GROUP BY a) AS dt
ON DUPLICATE KEY UPDATE k= a + 10;

SELECT * FROM t0;

DELETE FROM t0;
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

--echo # Not allowed: Reference column from a UNION query

--error ER_BAD_FIELD_ERROR
INSERT INTO t0
SELECT a FROM t1 UNION SELECT a FROM t2
ON DUPLICATE KEY UPDATE k= a + 10;

--echo # Allowed:: Wrap a UNION query in a derived table

INSERT INTO t0
SELECT a
FROM (SELECT a, COUNT(*) AS c FROM t1 GROUP BY a
      UNION
      SELECT a, COUNT(*) AS c FROM t2 GROUP BY a) AS dt
ON DUPLICATE KEY UPDATE k= dt.a + dt.c + 10;

SELECT * FROM t0;

DELETE FROM t0;
INSERT INTO t0 SELECT a FROM t1 UNION SELECT a FROM t2;

DROP TABLE t0, t1, t2;

--echo # Bug#25526439: Assertion failed: is_fixed_or_outer_ref(this)

CREATE TABLE t1 (
  a INTEGER NOT NULL
);
INSERT INTO t1 VALUES(0);

CREATE TABLE t2 (
  d INTEGER
);

--echo # Query from bug report
INSERT INTO t1(a) VALUES (1)
ON DUPLICATE KEY UPDATE a= (SELECT d FROM t2 GROUP BY 1);

SELECT * FROM t1;

--echo # Similar query with a simple query block
INSERT INTO t1(a) SELECT 1
ON DUPLICATE KEY UPDATE a= (SELECT d FROM t2 GROUP BY 1);

SELECT * FROM t1;

--echo # Similar query with a UNION
INSERT INTO t1(a) SELECT 1 UNION SELECT 2
ON DUPLICATE KEY UPDATE a= (SELECT d FROM t2 GROUP BY 1);

SELECT * FROM t1;

DROP TABLE t1, t2;

--echo # Bug#25071305: Assertion failed: first_execution ||
--echo #               !tl->is_view_or_derived() || tl->is_merged()

CREATE TABLE t1(a INTEGER);
CREATE TABLE t2(b INTEGER);
INSERT INTO t2 VALUES (1),(1);
INSERT INTO t1(a) VALUES (1)
ON DUPLICATE KEY UPDATE a= (SELECT b FROM (SELECT b FROM t2) AS w);
DROP TABLE t1, t2;

--echo # Bug#24716127: Incorrect behavior by insert statement with
--echo #               "on duplicate key update"

CREATE TABLE t1(a INTEGER, b INTEGER, PRIMARY KEY(a,b));
CREATE TABLE t2(c2 INTEGER NOT NULL PRIMARY KEY);
CREATE TABLE t3(c3 INTEGER NOT NULL PRIMARY KEY);

INSERT INTO t1 VALUES (1, 1);
INSERT INTO t2 VALUES (1), (2);
INSERT INTO t3 VALUES (1), (2);

INSERT INTO t1 VALUES (1,1)
ON DUPLICATE KEY UPDATE a= (SELECT c2
                            FROM t2 JOIN t3 ON c3 = c2
                            WHERE c2 = 1);

SELECT * FROM t1;

INSERT INTO t1
SELECT 1, 1 FROM t2
ON DUPLICATE KEY UPDATE a= t2.c2 + 100, b= t2.c2 + 100;

SELECT * FROM t1;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#28995498 INSERT IS FINE BUT "ON DUPLICATE UPDATE" UPDATING WRONG DATA
--echo #

CREATE TABLE t1 (pk VARCHAR(10) PRIMARY KEY, col VARCHAR(10));
INSERT INTO t1 VALUES (1 , "Carmen" ),(2 , "Martin" );
INSERT INTO t1 SELECT * FROM t1 AS source
  ON DUPLICATE KEY UPDATE t1.col = source.col;
SELECT * FROM t1 ;
DROP TABLE t1;

--echo # WL#6312: Referencing new row in INSERT .. VALUES .. ON DUPLICATE KEY UPDATE.

CREATE TABLE t0(a INT PRIMARY KEY, b INT);
CREATE TABLE t1(x INT PRIMARY KEY, y INT);

--echo # Allowed: Referencing VALUES from the update list.

INSERT INTO t0 VALUES (1, 3), (2, 3) AS n(a, b)
ON DUPLICATE KEY UPDATE b= t0.b + n.b;

SELECT * FROM t0;

INSERT INTO t0 VALUES (1, 3), (2, 3) AS n(a, b)
ON DUPLICATE KEY UPDATE b= t0.b + n.b;

SELECT * FROM t0;

--echo # Allowed: Not naming columns for VALUES table.

INSERT INTO t0 VALUES (1, 5), (2, 7) AS n
ON DUPLICATE KEY UPDATE b= t0.b + n.b;

SELECT * FROM t0;

--echo # Not allowed: Naming the VALUES table the same as the table inserted into.

--error ER_NONUNIQ_TABLE
INSERT INTO t0 VALUES (1, 5), (2, 7) AS t0
ON DUPLICATE KEY UPDATE b= t0.a;

--echo # Not allowed: Naming multiple VALUES columns the same.

--error ER_DUP_FIELDNAME
INSERT INTO t0 VALUES (1, 5), (2, 7) AS n(a, a)
ON DUPLICATE KEY UPDATE b= t0.a;

--echo # Not allowed: Unequal number of columns in VALUES table and inserted rows.

--error ER_VIEW_WRONG_LIST
INSERT INTO t0 VALUES (1, 5), (2, 7) AS n(a)
ON DUPLICATE KEY UPDATE b= t0.a;

--echo # Allowed: Referencing VALUES table from within a subquery.

INSERT INTO t1 VALUES (1, 50), (2, 100);

INSERT INTO t0 VALUES (1, 10), (2, 20) AS n
ON DUPLICATE KEY UPDATE b= (SELECT y FROM t1 WHERE x = n.a);

SELECT * FROM t0;

--echo # Allowed: Overriding the VALUES table name from within a subquery.

INSERT INTO t0 VALUES (1, 10) AS n(a, b)
ON DUPLICATE KEY UPDATE b= 20 + (SELECT n.y FROM t1 AS n WHERE n.x = t0.a);

SELECT * FROM t0;

--echo # Allowed: VALUES table name overriding table names.

CREATE TABLE n(a INT, b INT);

INSERT INTO t0 VALUES (1, 10) AS n(a, b)
ON DUPLICATE KEY UPDATE b= n.b;

SELECT * FROM t0;

--echo # Allowed: FROM clause in subquery overriding VALUES table name.

INSERT INTO n VALUES (1, 50);

INSERT INTO t0 VALUES (1, 20) AS n(a, b)
ON DUPLICATE KEY UPDATE b= (SELECT n.b FROM n);

SELECT * FROM t0;

--echo # Allowed: Discrepancy between specified insert columns and column names of VALUES table.

CREATE TABLE t2(a INT, b INT PRIMARY KEY, c INT);
INSERT INTO t2 VALUES (1, 10, 100);

INSERT INTO t2(b, c) VALUES (10, 20) AS n(a, b)
ON DUPLICATE KEY UPDATE c= n.b;

SELECT * FROM t2;

--echo # Allowed: Using the «INSERT .. SET» syntax.

DROP TABLE t0;
CREATE TABLE t0(a INT PRIMARY KEY, b INT);
INSERT INTO t0 VALUES (1, 10);

INSERT INTO t0 SET a=1, b=20 AS n
ON DUPLICATE KEY UPDATE b= n.b;

SELECT * FROM t0;

--echo # Allowed: Referring to the VALUES table from inside functions.

DROP TABLE t1;
CREATE TABLE t1(a BIGINT UNSIGNED PRIMARY KEY, b BIGINT UNSIGNED);

INSERT INTO t1 VALUES (45, 1) AS n
ON DUPLICATE KEY UPDATE b= IF(n.b > t1.b, n.b, t1.b);

SELECT * FROM t1;

INSERT INTO t1 VALUES (45, 2) AS n
ON DUPLICATE KEY UPDATE b= IF(n.b > t1.b, n.b, t1.b);

SELECT * FROM t1;

INSERT INTO t1 VALUES (45, 1) AS n
ON DUPLICATE KEY UPDATE b= IF(n.b > t1.b, n.b, t1.b);

SELECT * FROM t1;

--echo # Allowed: Using ODKU when inserting into a view.

DROP TABLE t1;

CREATE TABLE t1(a INT PRIMARY KEY DEFAULT 3, b INT);
CREATE VIEW v AS SELECT b FROM t1;

INSERT INTO t1 VALUES(3, 2);

INSERT INTO v VALUES(3) AS n
ON DUPLICATE KEY UPDATE b= n.b;

SELECT * FROM t1;
SELECT * FROM v;

--echo # Allowed: Creating a VALUES alias with no ODKU statement.

DROP TABLE t1;

CREATE TABLE t1(a INT PRIMARY KEY, b INT);

INSERT INTO t1 VALUES(1, 10) as n;

SELECT * FROM t1;

--echo # Not allowed: Referencing the VALUES table inside the VALUES clause.

--error ER_BAD_FIELD_ERROR
INSERT INTO t1 VALUES(n.a, 10) as n
ON DUPLICATE KEY UPDATE b= n.b;

--echo # If a table reference within an update expression does not find the
--echo # column in the VALUES table, it should look further in outer contexts.

DROP TABLE n;
CREATE TABLE n(x INT, y INT);

DROP TABLE t0;
CREATE TABLE t0(a INT PRIMARY KEY, b INT);

DELETE FROM t0;

INSERT INTO t0 VALUES(1, 10);
INSERT INTO n VALUES(1, 11);

INSERT INTO t0 VALUES(1, 19) as n(a, b)
ON DUPLICATE KEY UPDATE b= (SELECT n.y FROM n);

SELECT * FROM t0;

--echo # Allowed: Referring to the old row of the insert table from within an
--echo # ODKU update expression.

DELETE FROM t0;

INSERT INTO t0 VALUES(1, 10);
INSERT INTO t0 VALUES(2, 20);

INSERT INTO t0 VALUES(2, 29) as n
ON DUPLICATE KEY UPDATE b= t0.b+1;

SELECT * FROM t0;

DROP TABLE t0;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE n;
DROP VIEW v;

--echo # WL#6312: Referencing new row in INSERT .. VALUES .. ON DUPLICATE KEY UPDATE.
--echo # Compare old and new syntax (VALUES() vs. AS new) for regressions.

--echo # binlog_unsafe.test. More than one unique key.
--echo # Old syntax.
CREATE TABLE insert_2_keys (a INT UNIQUE KEY, b INT UNIQUE KEY);
INSERT INTO insert_2_keys values (1, 1);

INSERT INTO insert_2_keys VALUES (1, 2)
ON DUPLICATE KEY UPDATE a= VALUES(a) + 10, b= VALUES(b) + 10;

SELECT * FROM insert_2_keys;

DROP TABLE insert_2_keys;

--echo # New syntax.
CREATE TABLE insert_2_keys (a INT UNIQUE KEY, b INT UNIQUE KEY);
INSERT INTO insert_2_keys values (1, 1);

INSERT INTO insert_2_keys VALUES (1, 2) AS n
ON DUPLICATE KEY UPDATE a= n.a + 10, b= n.b + 10;

SELECT * FROM insert_2_keys;

DROP TABLE insert_2_keys;

--echo # json_functions.inc. JSON should work with INSERT .. ON DUPLICATE KEY UPDATE.
--echo # Old syntax.
CREATE TABLE t(id INT PRIMARY KEY, j JSON);
INSERT INTO t VALUES (1, '[1]')
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("a", VALUES(j));
SELECT * FROM t;
INSERT INTO t VALUES (1, '[1,2]')
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("ab", VALUES(j));
SELECT * FROM t;
INSERT INTO t VALUES (1, '[1,2,3]')
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("abc", VALUES(j));
SELECT * FROM t;
DROP TABLE t;

--echo # New syntax.
CREATE TABLE t(id INT PRIMARY KEY, j JSON);
INSERT INTO t VALUES (1, '[1]') AS n
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("a", n.j);
SELECT * FROM t;
INSERT INTO t VALUES (1, '[1,2]') AS n
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("ab", n.j);
SELECT * FROM t;
INSERT INTO t VALUES (1, '[1,2,3]') AS n
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("abc", n.j);
SELECT * FROM t;
DROP TABLE t;

--echo # errors.test. Subquery with VALUES table reference.
--echo # Old syntax.
CREATE TABLE t1(a INT);
CREATE TABLE t2(a INT PRIMARY KEY, b INT);

INSERT INTO t1 VALUES (10);

INSERT INTO t2(a, b) VALUES (1, 0);
INSERT INTO t2(a, b) VALUES (1, 0)
ON DUPLICATE KEY UPDATE b= (SELECT VALUES(a) + 2 FROM t1);

SELECT * FROM t2;

DROP TABLE t1;
DROP TABLE t2;

--echo # New syntax.
CREATE TABLE t1 (a INT);
CREATE TABLE t2(a INT PRIMARY KEY, b INT);

INSERT INTO t1 VALUES (10);

INSERT INTO t2(a, b) VALUES (1, 0);
INSERT INTO t2(a, b) VALUES (1, 0) AS n
ON DUPLICATE KEY UPDATE b= (SELECT n.a + 2 FROM t1);

SELECT * FROM t2;

DROP TABLE t1;
DROP TABLE t2;

--echo # func_test.test. New syntax should work inside functions.
--echo # Old syntax.
CREATE TABLE t1(a INT PRIMARY KEY, b INT);

INSERT INTO t1 VALUES (1, 2);
INSERT INTO t1 VALUES (1, 3)
ON DUPLICATE KEY UPDATE b= GREATEST(b, VALUES(b));

SELECT * FROM t1;

DROP TABLE t1;

--echo # New syntax.
CREATE TABLE t1(a INT PRIMARY KEY, b INT);

INSERT INTO t1 VALUES (1, 2);
INSERT INTO t1 VALUES (1, 3) AS n
ON DUPLICATE KEY UPDATE b= GREATEST(t1.b, n.b);

SELECT * FROM t1;

DROP TABLE t1;

--echo # type_blob-bug13901905_myisam.test. INSERT .. SET syntax should work with blobs.
--echo # Old syntax.
CREATE TABLE t1 (a INT, b BLOB, UNIQUE KEY(a));

INSERT INTO t1 SET b='11', a=0
ON DUPLICATE KEY UPDATE b= VALUES(a), a= values(b);

INSERT INTO t1 SET b='11', a=0
ON DUPLICATE KEY UPDATE b= VALUES(a), a= values(b);

SELECT * FROM t1;

DROP TABLE t1;

--echo # New syntax
CREATE TABLE t1 (a INT, b BLOB, UNIQUE KEY(a));

INSERT INTO t1 SET b='11', a=0 AS n
ON DUPLICATE KEY UPDATE b= n.a, a= n.b;

INSERT INTO t1 SET b='11', a=0 AS n
ON DUPLICATE KEY UPDATE b= n.a, a= n.b;

SELECT * FROM t1;

DROP TABLE t1;

--echo # insert-bug25361251.test. Text fields should work.
--echo # Old syntax.
CREATE TABLE t1(id INT NOT NULL, text1 TEXT, text2 TEXT, PRIMARY KEY (id));
INSERT INTO t1 VALUES (0, "x",  "x"), (1, "y",  "y");

INSERT INTO t1 (id, text1, text2) VALUES (0, "x", "y")
ON DUPLICATE KEY UPDATE text1 = VALUES(text1), text2 = VALUES(text2);

SELECT * FROM t1;

DROP TABLE t1;

--echo # New syntax.
CREATE TABLE t1(id INT NOT NULL, text1 TEXT, text2 TEXT, PRIMARY KEY (id));
INSERT INTO t1 VALUES (0, "x",  "x"), (1, "y",  "y");

INSERT INTO t1 (id, text1, text2) VALUES (0, "x", "y") AS n
ON DUPLICATE KEY UPDATE text1 = n.text1, text2 = n.text2;

SELECT * FROM t1;

DROP TABLE t1;

--echo # Bug#30051303: SERVER CRASHED WHILE RUNNING INSERT ON DUPLICATE KEY
--echo #               UPDATE OVER VIEW

CREATE TABLE t0(a INT PRIMARY KEY, b INT, c INT);
CREATE VIEW v AS SELECT t0.a AS va, t0.b AS vb, t0.c AS vc FROM t0;

INSERT INTO v(va, vb, vc) VALUES(1, 10, 100) AS n
ON DUPLICATE KEY UPDATE vc= 199;

SELECT * FROM t0;

DROP TABLE t0;
DROP VIEW v;
