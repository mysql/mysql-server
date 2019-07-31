SET TIME_ZONE = "+00:00";
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
--echo #
--echo # Test of errors for column data types that dont support function
--echo # defaults.
--echo #
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a BIT DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a TINYINT DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a SMALLINT DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a MEDIUMINT DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a INT DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a BIGINT DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a FLOAT DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a DECIMAL DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a DATE DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a TIME DEFAULT $current_timestamp );
--error ER_INVALID_DEFAULT
eval CREATE TABLE t1( a YEAR DEFAULT $current_timestamp );

--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a BIT ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a TINYINT ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a SMALLINT ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a MEDIUMINT ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a INT ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a BIGINT ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a FLOAT ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a DECIMAL ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a DATE ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a TIME ON UPDATE $current_timestamp );
--error ER_INVALID_ON_UPDATE
eval CREATE TABLE t1( a YEAR ON UPDATE $current_timestamp );

--echo #
--echo # Test that the default clause behaves like NOW() regarding time zones.
--echo #
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $timestamp NOT NULL DEFAULT $current_timestamp,
  c $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  d $timestamp NULL,
  e $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  f $datetime DEFAULT $current_timestamp,
  g $datetime ON UPDATE $current_timestamp,
  h $datetime
);

--echo # 2011-09-27 14:11:08 UTC
SET TIMESTAMP = 1317132668.654321;
SET @old_time_zone = @@TIME_ZONE;
SET TIME_ZONE = "+05:00";

eval INSERT INTO t1( d, h ) VALUES ( $now, $now );
SELECT * FROM t1;

--echo # 1989-05-13 01:02:03
SET TIMESTAMP = 611017323.543212;
eval UPDATE t1 SET d = $now, h = $now;
SELECT * FROM t1;

SET TIME_ZONE = @old_time_zone;
DROP TABLE t1;

--echo #
--echo # Test of several TIMESTAMP columns with different function defaults.
--echo #
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  c $timestamp NOT NULL DEFAULT $current_timestamp,
  d $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  e $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  f INT
);

--echo # 2011-04-19 07:22:02 UTC
SET TIMESTAMP = 1303197722.534231;

INSERT INTO t1 ( f ) VALUES (1);
SELECT * FROM t1;

--echo # 2011-04-19 07:23:18 UTC
SET TIMESTAMP = 1303197798.132435;

UPDATE t1 SET f = 2;
SELECT * FROM t1;

DROP TABLE t1;

--echo #
--echo # Test of inserted values out of order.
--echo #
eval CREATE TABLE t1 (
  a INT,
  b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  c $timestamp NOT NULL DEFAULT $current_timestamp,
  d $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  e $timestamp NULL,
  f $datetime,
  g $datetime DEFAULT $current_timestamp,
  h $datetime ON UPDATE $current_timestamp,
  i $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  j INT
);

--echo # 2011-04-19 07:22:02 UTC
SET TIMESTAMP = 1303197722.534231;

INSERT INTO t1 ( j, a ) VALUES ( 1, 1 );
SELECT * FROM t1;

DROP TABLE t1;

--echo #
--echo # Test of ON DUPLICATE KEY UPDATE
--echo #
eval CREATE TABLE t1 (
  a INT PRIMARY KEY,
  b INT,
  c $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  d $timestamp NOT NULL DEFAULT $current_timestamp,
  e $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  f $timestamp NOT NULL DEFAULT '1986-09-27 03:00:00.098765',
  g $timestamp NULL,
  h $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  i $datetime DEFAULT $current_timestamp,
  j $datetime ON UPDATE $current_timestamp,
  k $datetime NULL,
  l $datetime DEFAULT '1986-09-27 03:00:00.098765'
);

--echo # 1977-12-21 23:00:00 UTC
SET TIMESTAMP = 251593200.192837;
INSERT INTO t1(a) VALUES (1) ON DUPLICATE KEY UPDATE b = 2;
SELECT * FROM t1;

--echo # 1975-05-21 23:00:00 UTC
SET TIMESTAMP = 169945200.918273;
INSERT INTO t1(a) VALUES (1) ON DUPLICATE KEY UPDATE b = 2;
SELECT * FROM t1;

--echo # 1973-08-14 09:11:22 UTC
SET TIMESTAMP = 114167482.534231;
INSERT INTO t1(a) VALUES (2) ON DUPLICATE KEY UPDATE b = 2;
SELECT * FROM t1;

DROP TABLE t1;

eval CREATE TABLE t1 ( a INT PRIMARY KEY, b INT, c $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp );

--echo # 2011-04-19 07:23:18 UTC
SET TIMESTAMP = 1303197798.945156;

INSERT INTO t1 VALUES
  (1, 0, '2001-01-01 01:01:01.111111'),
  (2, 0, '2002-02-02 02:02:02.222222'),
  (3, 0, '2003-03-03 03:03:03.333333');
SELECT * FROM t1;

UPDATE t1 SET b = 2, c = c WHERE a = 2;
SELECT * FROM t1;

INSERT INTO t1 (a) VALUES (4);
SELECT * FROM t1;

UPDATE t1 SET c = '2004-04-04 04:04:04.444444' WHERE a = 4;
SELECT * FROM t1;

INSERT INTO t1 ( a ) VALUES ( 3 ), ( 5 ) ON DUPLICATE KEY UPDATE b = 3, c = c;
SELECT * FROM t1;

INSERT INTO t1 (a, c) VALUES
  (4, '2004-04-04 00:00:00.444444'),
  (6, '2006-06-06 06:06:06.666666')
ON DUPLICATE KEY UPDATE b = 4;

SELECT * FROM t1;

DROP TABLE t1;


--echo #
--echo # Test of REPLACE INTO executed as UPDATE.
--echo #
eval CREATE TABLE t1 (
  a INT PRIMARY KEY,
  b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  c $datetime  DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  d $timestamp NOT NULL DEFAULT $current_timestamp,
  e $datetime  DEFAULT $current_timestamp,
  f $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  g $datetime  ON UPDATE $current_timestamp,
  h $timestamp NULL,
  i $datetime
);

--echo # 1970-09-21 09:11:12 UTC
SET TIMESTAMP = 22756272.163584;

REPLACE INTO t1 ( a ) VALUES ( 1 );
SELECT * FROM t1;

--echo # 1970-11-10 14:16:17 UTC
SET TIMESTAMP = 27094577.852954;


REPLACE INTO t1 ( a ) VALUES ( 1 );
SELECT * FROM t1;

DROP TABLE t1;


--echo #
--echo # Test of insertion of NULL, DEFAULT and an empty row for DEFAULT
--echo # CURRENT_TIMESTAMP.
--echo #
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $datetime DEFAULT $current_timestamp,
  c INT
);

--echo # 2011-04-20 09:53:41 UTC
SET TIMESTAMP = 1303293221.163578;

INSERT INTO t1 VALUES (NULL, NULL, 1), (DEFAULT, DEFAULT, 2);
INSERT INTO t1 ( a, b, c ) VALUES (NULL, NULL, 3), (DEFAULT, DEFAULT, 4);
SELECT * FROM t1;

SET TIME_ZONE = "+03:00";
SELECT * FROM t1;
SET TIME_ZONE = "+00:00";

DROP TABLE t1;

--echo # 2011-04-20 07:05:39 UTC
SET TIMESTAMP = 1303283139.195624;
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT '2010-10-11 12:34:56' ON UPDATE $current_timestamp,
  b $datetime DEFAULT '2010-10-11 12:34:56'
);

INSERT INTO t1          VALUES (NULL, NULL), (DEFAULT, DEFAULT);
INSERT INTO t1 ( a, b ) VALUES (NULL, NULL), (DEFAULT, DEFAULT);
SELECT * FROM t1;

DROP TABLE t1;

--echo # 2011-04-20 09:53:41 UTC
SET TIMESTAMP = 1303293221.136952;

eval CREATE TABLE t1 (
a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
b $timestamp NOT NULL DEFAULT $current_timestamp,
c $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
d $timestamp NOT NULL DEFAULT '1986-09-27 03:00:00.098765',
e $timestamp NULL,
f $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp,
g $datetime DEFAULT $current_timestamp,
h $datetime ON UPDATE $current_timestamp,
i $datetime NULL,
j $datetime DEFAULT '1986-09-27 03:00:00.098765'
);

INSERT INTO t1 VALUES ();

INSERT INTO t1 SELECT NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL;

SELECT * FROM t1;

DROP TABLE t1;

--echo #
--echo # Test of multiple-table UPDATE for DEFAULT CURRENT_TIMESTAMP
--echo #
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $datetime DEFAULT $current_timestamp,
  c INT
);

INSERT INTO t1 ( c ) VALUES (1);
SELECT * FROM t1;

--echo # 2011-04-20 17:06:13 UTC
SET TIMESTAMP = 1303311973.163587;

UPDATE t1 t11, t1 t12 SET t11.c = 1;
SELECT * FROM t1;

UPDATE t1 t11, t1 t12 SET t11.c = 2;

SELECT * FROM t1;

DROP TABLE t1;

eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT $current_timestamp,
  b $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  c $datetime  DEFAULT   $current_timestamp,
  d $datetime  ON UPDATE $current_timestamp,
  e INT
);

eval CREATE TABLE t2 (
  f INT,
  g $datetime  ON UPDATE $current_timestamp,
  h $datetime  DEFAULT   $current_timestamp,
  i $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  j $timestamp NOT NULL DEFAULT $current_timestamp
);

--echo # 1995-03-11 00:02:03 UTC
SET TIMESTAMP = 794880123.195676;

INSERT INTO t1 ( e ) VALUES ( 1 ), ( 2 );

INSERT INTO t2 ( f ) VALUES ( 1 ), ( 2 );

SELECT * FROM t1;
SELECT * FROM t2;

--echo # 1980-12-13 02:02:01 UTC
SET TIMESTAMP = 345520921.196755;

UPDATE t1, t2 SET t1.e = 3, t2.f = 4;

SELECT * FROM t1;
SELECT * FROM t2;

DROP TABLE t1, t2;

--echo #
--echo # Test of multiple table update with temporary table and on the fly.
--echo #
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  b $datetime ON UPDATE $current_timestamp,
  c INT,
  d INT
);

eval CREATE TABLE t2 (
  a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  b $datetime ON UPDATE $current_timestamp,
  c INT KEY,
  d INT
);

INSERT INTO t1 ( c ) VALUES (1), (2);
INSERT INTO t2 ( c ) VALUES (1), (2);

--echo # Test of multiple table update done on the fly
--echo # 2011-04-20 15:06:13 UTC
SET TIMESTAMP = 1303311973.194685;
UPDATE t1 JOIN t2 USING ( c ) SET t2.d = 1;
SELECT * FROM t1;
SELECT * FROM t2;

--echo # Test of multiple table update done with temporary table.
--echo # 1979-01-15 03:02:01
SET TIMESTAMP = 285213721.134679;
UPDATE t1 JOIN t2 USING ( c ) SET t1.d = 1;
SELECT * FROM t1;
SELECT * FROM t2;

DROP TABLE t1, t2;


--echo #
--echo # Test of ON UPDATE CURRENT_TIMESTAMP.
--echo #
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  b $datetime ON UPDATE $current_timestamp,
  c INT
);

--echo # 2011-04-20 09:53:41 UTC
SET TIMESTAMP = 1303293221.794613;

INSERT INTO t1 ( c ) VALUES ( 1 );
SELECT * FROM t1;

UPDATE t1 SET c = 1;
SELECT * FROM t1;

UPDATE t1 SET c = 2;
SELECT * FROM t1;

--echo #
--echo # Test of multiple-table UPDATE for ON UPDATE CURRENT_TIMESTAMP
--echo #
--echo # 2011-04-20 15:06:13 UTC
SET TIMESTAMP = 1303311973.534231;

UPDATE t1 t11, t1 t12 SET t11.c = 2;
SELECT * FROM t1;

UPDATE t1 t11, t1 t12 SET t11.c = 3;
SELECT * FROM t1;

DROP TABLE t1;

--echo #
--echo # Test of a multiple-table update where only one table is updated and
--echo # the updated table has a primary key.
--echo #
eval CREATE TABLE t1 ( a INT, b INT, PRIMARY KEY (a) );
INSERT INTO t1 VALUES (1, 1),(2, 2),(3, 3),(4, 4);

eval CREATE TABLE t2 ( a INT, b INT );
INSERT INTO t2 VALUES (1, 1),(2, 2),(3, 3),(4, 4),(5, 5);

UPDATE t1, t2 SET t1.b = 100 WHERE t1.a = t2.a;

SELECT * FROM t1;
SELECT * FROM t2;

DROP TABLE t1, t2;

--echo #
--echo # Test of ALTER TABLE, reordering columns.
--echo #
eval CREATE TABLE t1 ( a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp, b INT );eval ALTER TABLE t1 MODIFY a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp AFTER b;
SHOW CREATE TABLE t1;
DROP TABLE t1;

eval CREATE TABLE t1 ( a INT, b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp, c $timestamp NULL );eval ALTER TABLE t1 MODIFY b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp FIRST;
SHOW CREATE TABLE t1;
DROP TABLE t1;

eval CREATE TABLE t1 ( a INT, b $timestamp NULL );eval ALTER TABLE t1 MODIFY b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp FIRST;
SHOW CREATE TABLE t1;
DROP TABLE t1;

eval CREATE TABLE t1 ( a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp, b $timestamp NULL );eval ALTER TABLE t1 MODIFY a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' AFTER b;
SHOW CREATE TABLE t1;
DROP TABLE t1;

eval CREATE TABLE t1 ( a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp, b $timestamp NULL );eval ALTER TABLE t1 MODIFY a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' AFTER b;
SHOW CREATE TABLE t1;
DROP TABLE t1;

eval CREATE TABLE t1 ( a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $now, b INT, c $timestamp NULL );
SHOW CREATE TABLE t1;eval ALTER TABLE t1 MODIFY a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp AFTER b;
SHOW CREATE TABLE t1;
DROP TABLE t1;

eval CREATE TABLE t1 ( a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $now, b INT, c $timestamp NULL );eval ALTER TABLE t1 MODIFY c $timestamp NULL FIRST;
SHOW CREATE TABLE t1;
DROP TABLE t1;

eval CREATE TABLE t1 ( a $timestamp NOT NULL DEFAULT $now ON UPDATE $current_timestamp, b INT, c $timestamp NULL );
SHOW CREATE TABLE t1;eval ALTER TABLE t1 MODIFY a $timestamp NOT NULL DEFAULT $now ON UPDATE $current_timestamp AFTER b;
SHOW CREATE TABLE t1;
DROP TABLE t1;

eval CREATE TABLE t1 ( a $timestamp NOT NULL DEFAULT $now ON UPDATE $current_timestamp, b INT, c $timestamp NULL );eval ALTER TABLE t1 MODIFY c $timestamp NULL FIRST;
SHOW CREATE TABLE t1;
DROP TABLE t1;


--echo #
--echo # Test of ALTER TABLE, adding columns.
--echo #
eval CREATE TABLE t1 ( a INT );
eval ALTER TABLE t1 ADD COLUMN b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp;
SHOW CREATE TABLE t1;
DROP TABLE t1;

--echo #
--echo # Test of INSERT SELECT.
--echo #
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  c $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  d $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp
);

eval CREATE TABLE t2 (
  placeholder1 INT,
  placeholder2 INT,
  placeholder3 INT,
  placeholder4 INT,
  a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  c $datetime,
  d $datetime
);

--echo # 1977-08-16 15:30:01 UTC
SET TIMESTAMP = 240589801.654312;

INSERT INTO t2 (a, b, c, d) VALUES (
  '1977-08-16 15:30:01.123456',
  '1977-08-16 15:30:01.234567',
  '1977-08-16 15:30:01.345678',
  '1977-08-16 15:30:01.456789'
);

--echo # 1986-09-27 01:00:00 UTC
SET TIMESTAMP = 528166800.132435;

INSERT INTO t1 ( a, c ) SELECT a, c FROM t2;

SELECT * FROM t1;

DROP TABLE t1, t2;

--echo #
--echo # Test of CREATE TABLE SELECT.
--echo #
--echo # We test that the columns of the source table are used to determine
--echo # function defaults for the receiving table.
--echo #

--echo # 1970-04-11 20:13:57 UTC
SET TIMESTAMP = 8712837.657898;
eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $timestamp NOT NULL DEFAULT $current_timestamp,
  c $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  d $timestamp NOT NULL DEFAULT '1986-09-27 03:00:00.098765',
  e $timestamp NULL,
  f $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  g $datetime DEFAULT $current_timestamp,
  h $datetime ON UPDATE $current_timestamp,
  i $datetime NULL,
  j $datetime DEFAULT '1986-09-27 03:00:00.098765'
);

INSERT INTO t1 VALUES ();

--echo # 1971-01-31 21:13:57 UTC
SET TIMESTAMP = 34200837.164937;

eval CREATE TABLE t2  SELECT a FROM t1; SHOW CREATE TABLE t2;  SELECT * FROM t2;
eval CREATE TABLE t3  SELECT b FROM t1; SHOW CREATE TABLE t3;  SELECT * FROM t3;
eval CREATE TABLE t4  SELECT c FROM t1; SHOW CREATE TABLE t4;  SELECT * FROM t4;
eval CREATE TABLE t5  SELECT d FROM t1; SHOW CREATE TABLE t5;  SELECT * FROM t5;
eval CREATE TABLE t6  SELECT e FROM t1; SHOW CREATE TABLE t6;  SELECT * FROM t6;
eval CREATE TABLE t7  SELECT f FROM t1; SHOW CREATE TABLE t7;  SELECT * FROM t7;
eval CREATE TABLE t8  SELECT g FROM t1; SHOW CREATE TABLE t8;  SELECT * FROM t8;
eval CREATE TABLE t9  SELECT h FROM t1; SHOW CREATE TABLE t9;  SELECT * FROM t9;
eval CREATE TABLE t10 SELECT i FROM t1; SHOW CREATE TABLE t10; SELECT * FROM t10;
eval CREATE TABLE t11 SELECT j FROM t1; SHOW CREATE TABLE t11; SELECT * FROM t11;

eval CREATE TABLE t12 (
  k $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  l $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  m $timestamp NOT NULL DEFAULT $current_timestamp,
  n $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  o $timestamp NOT NULL DEFAULT '1986-09-27 03:00:00.098765',
  p $timestamp NULL,
  q $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  r $datetime DEFAULT $current_timestamp,
  s $datetime ON UPDATE $current_timestamp,
  t $datetime NULL,
  u $datetime DEFAULT '1986-09-27 03:00:00.098765'
)
SELECT * FROM t1;

SHOW CREATE TABLE t12;

--echo # No function default for non-column expressions:
eval CREATE TABLE t13  SELECT COALESCE(a,a) FROM t1; SHOW CREATE TABLE t13;  SELECT * FROM t13;

DROP TABLE t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13;

--echo # 1970-04-11 20:13:57 UTC
SET TIMESTAMP = 8712837.164953;
eval CREATE TABLE t1 (
  a $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $datetime DEFAULT $current_timestamp,
  c $datetime ON UPDATE $current_timestamp,
  d $datetime NULL,
  e $datetime DEFAULT '1986-09-27 03:00:00.098765'
);

INSERT INTO t1 VALUES ();

--echo # 1971-01-31 20:13:57 UTC
SET TIMESTAMP = 34200837.915736;

eval CREATE TABLE t2 SELECT a FROM t1;
SHOW CREATE TABLE t2;
SELECT * FROM t2;

eval CREATE TABLE t3 SELECT b FROM t1;
SHOW CREATE TABLE t3;
SELECT * FROM t3;

eval CREATE TABLE t4 SELECT c FROM t1;
SHOW CREATE TABLE t4;
SELECT * FROM t4;

eval CREATE TABLE t5 SELECT d FROM t1;
SHOW CREATE TABLE t5;
SELECT * FROM t5;

eval CREATE TABLE t6 SELECT e FROM t1;
SHOW CREATE TABLE t6;
SELECT * FROM t6;

--echo # Test CREATE TABLE LIKE
CREATE TABLE t7 LIKE t1;
SHOW CREATE TABLE t7;

DROP TABLE t1, t2, t3, t4, t5, t6, t7;

--echo #
--echo # Test of a CREATE TABLE SELECT that also declared columns. In this case
--echo # the function default for them should be activated during the execution of the
--echo # CREATE TABLE statement.
--echo #
--echo # 1970-01-01 03:16:40
SET TIMESTAMP = 1000.987654;
eval CREATE TABLE t1 ( a INT );
INSERT INTO t1 VALUES ( 1 ), ( 2 );

eval CREATE TABLE t2 ( b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp) SELECT a FROM t1;

SHOW CREATE TABLE t2;
SET TIMESTAMP = 2000.876543;
INSERT INTO t2( a ) VALUES ( 3 );
SELECT * FROM t2;

DROP TABLE t1, t2;

--echo #
--echo # Test of updating a view.
--echo #
eval CREATE TABLE t1 ( a INT, b $datetime DEFAULT $current_timestamp );
eval CREATE TABLE t2 ( a INT, b $datetime ON UPDATE $current_timestamp );

eval CREATE VIEW v1 AS SELECT * FROM t1;
SHOW CREATE VIEW v1;

eval CREATE VIEW v2 AS SELECT * FROM t2;
SHOW CREATE VIEW v2;

--echo # 1971-01-31 21:13:57 UTC
SET TIMESTAMP = 34200837.348564;

INSERT INTO v1 ( a ) VALUES ( 1 );
INSERT INTO v2 ( a ) VALUES ( 1 );

SELECT * FROM t1;
SELECT * FROM v1;

SELECT * FROM t2;
SELECT * FROM v2;

--echo # 1970-04-11 20:13:57 UTC
SET TIMESTAMP = 8712837.567332;
UPDATE v1 SET a = 2;
UPDATE v2 SET a = 2;

SELECT * FROM t1;
SELECT * FROM v1;

SELECT * FROM t2;
SELECT * FROM v2;

DROP VIEW v1, v2;
DROP TABLE t1, t2;

--echo #
--echo # Test with stored procedures.
--echo #
eval CREATE TABLE t1 (
  a INT,
  b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  c $timestamp NOT NULL DEFAULT $current_timestamp,
  d $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  e $timestamp NULL,
  f $datetime DEFAULT $current_timestamp,
  g $datetime ON UPDATE $current_timestamp
);
CREATE PROCEDURE p1() INSERT INTO test.t1( a ) VALUES ( 1 );
CREATE PROCEDURE p2() UPDATE t1 SET a = 2 WHERE a = 1;

--echo # 1971-01-31 20:13:57 UTC
SET TIMESTAMP = 34200837.876544;
CALL p1();
SELECT * FROM t1;

--echo # 1970-04-11 21:13:57 UTC
SET TIMESTAMP = 8712837.143546;
CALL p2();
SELECT * FROM t1;

DROP PROCEDURE p1;
DROP PROCEDURE p2;
DROP TABLE t1;

--echo #
--echo # Test with triggers.
--echo #
eval CREATE TABLE t1 (
  a INT,
  b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  c $timestamp NOT NULL DEFAULT $current_timestamp,
  d $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  e $timestamp NULL,
  f $datetime,
  g $datetime DEFAULT $current_timestamp,
  h $datetime ON UPDATE $current_timestamp,
  i $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp
);

eval CREATE TABLE t2 ( a INT );

DELIMITER |;
eval CREATE TRIGGER t2_trg BEFORE INSERT ON t2 FOR EACH ROW
BEGIN
  INSERT INTO t1 ( a ) VALUES ( 1 );
END|
DELIMITER ;|

--echo # 1971-01-31 21:13:57 UTC
SET TIMESTAMP = 34200837.978675;

INSERT INTO t2 ( a ) VALUES ( 1 );
SELECT * FROM t1;

DROP TRIGGER t2_trg;

DELIMITER |;
eval CREATE TRIGGER t2_trg BEFORE INSERT ON t2 FOR EACH ROW
BEGIN
  UPDATE t1 SET a = 2;
END|
DELIMITER ;|

--echo # 1970-04-11 21:13:57 UTC
SET TIMESTAMP = 8712837.456789;

INSERT INTO t2 ( a ) VALUES ( 1 );
SELECT * FROM t1;

DROP TABLE t1, t2;

--echo #
--echo # Test where the assignment target is not a column.
--echo #
eval CREATE TABLE t1 ( a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp );
eval CREATE TABLE t2 ( a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp );
eval CREATE TABLE t3 ( a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp );
eval CREATE TABLE t4 ( a $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp );

eval CREATE VIEW v1       AS SELECT a COLLATE latin1_german1_ci AS b FROM t1;
eval CREATE VIEW v2 ( b ) AS SELECT a COLLATE latin1_german1_ci      FROM t2;
eval CREATE VIEW v3       AS SELECT a COLLATE latin1_german1_ci AS b FROM t3;
eval CREATE VIEW v4 ( b ) AS SELECT a COLLATE latin1_german1_ci      FROM t4;

INSERT INTO v1 ( b ) VALUES ( '2007-10-24 00:03:34.010203' );
SELECT a FROM t1;

INSERT INTO v2 ( b ) VALUES ( '2007-10-24 00:03:34.010203' );
SELECT a FROM t2;

INSERT INTO t3 VALUES ();
UPDATE v3 SET b = '2007-10-24 00:03:34.010203';
SELECT a FROM t3;

INSERT INTO t4 VALUES ();
UPDATE v4 SET b = '2007-10-24 00:03:34.010203';
SELECT a FROM t4;

DROP VIEW  v1, v2, v3, v4;
DROP TABLE t1, t2, t3, t4;

--echo #
--echo # Test of LOAD DATA/XML INFILE
--echo # This tests behavior of function defaults for TIMESTAMP and DATETIME
--echo # columns. during LOAD ... INFILE.
--echo # As can be seen here, a TIMESTAMP column with only ON UPDATE
--echo # CURRENT_TIMESTAMP will still have CURRENT_TIMESTAMP inserted on LOAD
--echo # ... INFILE if the value is missing. For DATETIME columns a NULL value
--echo # is inserted instead.
--echo #

eval CREATE TABLE t1 (
  a INT,
  b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  c $timestamp NOT NULL DEFAULT $current_timestamp,
  d $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  e $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  f $datetime,
  g $datetime DEFAULT $current_timestamp,
  h $datetime ON UPDATE $current_timestamp,
  i $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp
);

eval CREATE TABLE t2 (
  a $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  b $timestamp NOT NULL DEFAULT $current_timestamp,
  c $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $current_timestamp,
  d $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  e $datetime NOT NULL,
  f $datetime NOT NULL DEFAULT '1977-01-02 12:13:14',
  g $datetime DEFAULT $current_timestamp NOT NULL,
  h $datetime ON UPDATE $current_timestamp NOT NULL,
  i $datetime DEFAULT $current_timestamp ON UPDATE $current_timestamp NOT NULL
);

SELECT 1 INTO OUTFILE 't3.dat' FROM dual;

SELECT NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
INTO OUTFILE 't4.dat'
FROM dual;

SELECT 1, 2 INTO OUTFILE 't5.dat' FROM dual;

--echo # Mon Aug 1 15:11:19 2011 UTC
SET TIMESTAMP = 1312211479.918273;

LOAD DATA INFILE 't3.dat' INTO TABLE t1;
--query_vertical SELECT * FROM t1

LOAD DATA INFILE 't4.dat' INTO TABLE t2;
SELECT a FROM t2;
SELECT b FROM t2;
SELECT c FROM t2;
SELECT d FROM t2;
--echo # As shown here, supplying a NULL value to a non-nullable
--echo # column with no default value results in the zero date.
SELECT e FROM t2;
--echo # As shown here, supplying a NULL value to a non-nullable column with a
--echo # default value results in the zero date.
SELECT f FROM t2;
--echo # As shown here, supplying a NULL value to a non-nullable column with a
--echo # default function results in the zero date.
SELECT g FROM t2;
--echo # As shown here, supplying a NULL value to a non-nullable DATETIME ON
--echo # UPDATE CURRENT_TIMESTAMP column with no default value results in the
--echo # zero date.
SELECT h FROM t2;
SELECT i FROM t2;

DELETE FROM t1;
DELETE FROM t2;

--echo # Read t3 file into t1
--echo # The syntax will cause a different code path to be taken
--echo # (read_fixed_length()) than under the LOAD ... INTO TABLE t1 command
--echo # above. The code in this path is copy-pasted code from the path taken
--echo # under the syntax used in the previous LOAD command.
LOAD DATA INFILE 't3.dat' INTO TABLE t1
FIELDS TERMINATED BY '' ENCLOSED BY '';

SELECT b FROM t1;
SELECT c FROM t1;
SELECT d FROM t1;
SELECT e FROM t1;
--echo # Yes, a missing field cannot be NULL using this syntax, so it will
--echo # zero date instead. Says a comment in read_fixed_length() : "No fields
--echo # specified in fields_vars list can be NULL in this format."
--echo # It appears to be by design. This is inconsistent with LOAD DATA INFILE
--echo # syntax in previous test.
SELECT f FROM t1;
SELECT g FROM t1;
--echo # See comment above "SELECT f FROM f1".
SELECT h FROM t1;
SELECT i FROM t1;
DELETE FROM t1;

LOAD DATA INFILE 't5.dat' INTO TABLE t1 ( a, @dummy );
SELECT * FROM t1;
SELECT @dummy;
DELETE FROM t1;

LOAD DATA INFILE 't3.dat' INTO TABLE t1 ( a ) SET c = '2005-06-06 08:09:10';
SELECT * FROM t1;
DELETE FROM t1;

LOAD DATA INFILE 't3.dat' INTO TABLE t1 ( a ) SET g = '2005-06-06 08:09:10';
SELECT * FROM t1;
DELETE FROM t1;

--echo # Load a static XML file
LOAD XML INFILE '../../std_data/onerow.xml' INTO TABLE t1
ROWS IDENTIFIED BY '<row>';

--echo Missing tags are treated as NULL
--query_vertical SELECT * FROM t1

DROP TABLE t1, t2;

let $MYSQLD_DATADIR= `select @@datadir`;
remove_file $MYSQLD_DATADIR/test/t3.dat;
remove_file $MYSQLD_DATADIR/test/t4.dat;
remove_file $MYSQLD_DATADIR/test/t5.dat;


--echo #
--echo # Similar LOAD DATA tests in another form
--echo #
--echo # All of this test portion has been run on a pre-WL5874 trunk
--echo # (except that like_b and like_c didn't exist) and all result
--echo # differences are a bug.
--echo # Regarding like_b its definition is the same as b's except
--echo # that the constant default is replaced with a function
--echo # default. Our expectation is that like_b would behave
--echo # like b: if b is set to NULL, or set to 0000-00-00, or set to
--echo # its default, then the same should apply to like_b. Same for
--echo # like_c vs c.

--echo # Mon Aug 1 15:11:19 2011 UTC
SET TIMESTAMP = 1312211479.089786;

SELECT 1 INTO OUTFILE "file1.dat" FROM dual;
SELECT NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
 INTO OUTFILE "file2.dat" FROM dual;

--echo # Too short row

eval
CREATE TABLE t1 (
  dummy INT,
  a $datetime NULL DEFAULT NULL,
  b $datetime NULL DEFAULT "2011-11-18",
  like_b $datetime NULL DEFAULT $current_timestamp,
  c $datetime NOT NULL DEFAULT "2011-11-18",
  like_c $datetime NOT NULL DEFAULT $current_timestamp,
  d $timestamp NULL DEFAULT "2011-05-03" ON UPDATE $current_timestamp,
  e $timestamp NOT NULL DEFAULT "2011-05-03",
  f $timestamp NOT NULL DEFAULT $current_timestamp,
  g $timestamp NULL DEFAULT NULL,
  h INT NULL,
  i INT NOT NULL DEFAULT 42
);

--echo # There is no promotion
SHOW CREATE TABLE t1;

LOAD DATA INFILE "file1.dat" INTO table t1;

--echo # It is strange that "like_b" gets NULL when "b" gets 0. But
--echo # this is consistent with how "a" gets NULL when "b" gets 0,
--echo # with how "g" gets NULL when "d" gets 0, and with how "h" gets
--echo # NULL when "i" gets 0. Looks like "DEFAULT
--echo # <non-NULL-constant>" is changed to 0, whereas DEFAULT NULL
--echo # and DEFAULT NOW are changed to NULL.
--query_vertical SELECT * FROM t1
delete from t1;

alter table t1
modify f TIMESTAMP NULL default CURRENT_TIMESTAMP;

--echo # There is no promotion
SHOW CREATE TABLE t1;

LOAD DATA INFILE "file1.dat" INTO table t1;

--query_vertical SELECT * FROM t1
delete from t1;

drop table t1;

--echo # Conclusion derived from trunk's results:
--echo # DATETIME DEFAULT <non-NULL-constant> (b,c) gets 0000-00-00,
--echo # DATETIME DEFAULT NULL (a) gets NULL,
--echo # TIMESTAMP NULL DEFAULT <non-NULL-constant> (d) gets 0000-00-00,
--echo # TIMESTAMP NULL DEFAULT NULL (g) gets NULL,
--echo # TIMESTAMP NULL DEFAULT NOW (f after ALTER) gets NULL,
--echo # TIMESTAMP NOT NULL (f before ALTER, e) gets NOW.

--echo ### Loading NULL ###

eval
CREATE TABLE t1 (
  dummy INT,
  a $datetime NULL DEFAULT NULL,
  b $datetime NULL DEFAULT "2011-11-18",
  like_b $datetime NULL DEFAULT $current_timestamp,
  c $datetime NOT NULL DEFAULT "2011-11-18",
  like_c $datetime NOT NULL DEFAULT $current_timestamp,
  d $timestamp NULL DEFAULT "2011-05-03" ON UPDATE $current_timestamp,
  e $timestamp NOT NULL DEFAULT "2011-05-03",
  f $timestamp NOT NULL DEFAULT $current_timestamp,
  g $timestamp NULL DEFAULT NULL,
  h INT NULL,
  i INT NOT NULL DEFAULT 42
);

--echo # There is no promotion
SHOW CREATE TABLE t1;

LOAD DATA INFILE "file2.dat" INTO table t1;

--query_vertical SELECT * FROM t1
delete from t1;

alter table t1
modify f TIMESTAMP NULL default CURRENT_TIMESTAMP;

--echo # There is no promotion
SHOW CREATE TABLE t1;

LOAD DATA INFILE "file2.dat" INTO table t1;

--query_vertical SELECT * FROM t1
delete from t1;

--echo # Conclusion derived from trunk's results:
--echo # DATETIME NULL (a,b) gets NULL,
--echo # DATETIME NOT NULL (c) gets 0000-00-00,
--echo # TIMESTAMP NULL (d,f,g) gets NULL,
--echo # TIMESTAMP NOT NULL (e) gets NOW.

drop table t1;
remove_file $MYSQLD_DATADIR/test/file1.dat;
remove_file $MYSQLD_DATADIR/test/file2.dat;

--echo #
--echo # Test of updatable views with check options.
--echo # Test case provides coverage for Bug#29834066 "DEFAULT EXPRESSION CAN
--echo # BREAK VIEW CHECK OPTION".
--echo #
SET TIME_ZONE = "+03:00";
--echo # 1970-01-01 03:16:40
SET TIMESTAMP = 1000.123456;

eval CREATE TABLE t1 ( a INT, b $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp);

SHOW CREATE TABLE t1;

INSERT INTO t1 ( a ) VALUES ( 1 );

SELECT * FROM t1;

eval CREATE VIEW v1 AS SELECT * FROM t1 WHERE b <= '1970-01-01 03:16:40.123456'
WITH CHECK OPTION;

SELECT * FROM v1;

--echo # 1970-01-01 03:33:20
SET TIMESTAMP = 2000.000234;

--error ER_VIEW_CHECK_FAILED
UPDATE v1 SET a = 2;
SELECT * FROM t1;

DROP VIEW v1;
DROP TABLE t1;

eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT '1973-08-14 09:11:22.089786' ON UPDATE $current_timestamp,
  c INT KEY
);
--echo # 1973-08-14 09:11:22 UTC
SET TIMESTAMP = 114167482.534231;
INSERT INTO t1 ( c ) VALUES ( 1 );

eval CREATE VIEW v1 AS
SELECT *
FROM t1
WHERE a >= '1973-08-14 09:11:22'
WITH LOCAL CHECK OPTION;

SELECT * FROM v1;

SET TIMESTAMP = 1.126789;

--error ER_VIEW_CHECK_FAILED
INSERT INTO v1 ( c ) VALUES ( 1 ) ON DUPLICATE KEY UPDATE c = 2;

SELECT * FROM v1;

DROP VIEW v1;
DROP TABLE t1;

--echo #
--echo # Bug 13095459 - MULTI-TABLE UPDATE MODIFIES A ROW TWICE
--echo #
eval CREATE TABLE t1 (
  a INT,
  b INT,
  ts $timestamp NOT NULL DEFAULT $current_timestamp ON UPDATE $current_timestamp,
  PRIMARY KEY ( a, ts )
);
INSERT INTO t1( a, b, ts ) VALUES ( 1, 0, '2000-09-28 17:44:34' );

CREATE TABLE t2 ( a INT );
INSERT INTO t2 VALUES ( 1 );

UPDATE t1 STRAIGHT_JOIN t2
SET t1.b = t1.b + 1
WHERE t1.a = 1 AND t1.ts >= '2000-09-28 00:00:00';

SELECT b FROM t1;

DROP TABLE t1, t2;

--echo #
--echo # Bug#11745578: 17392: ALTER TABLE ADD COLUMN TIMESTAMP DEFAULT
--echo # CURRENT_TIMESTAMP INSERTS ZERO
--echo #
SET timestamp = 1000;

CREATE TABLE t1 ( b INT );
INSERT INTO t1 VALUES (1),(2);

eval ALTER TABLE t1 ADD COLUMN a6 $datetime DEFAULT $now ON UPDATE $now FIRST;
eval ALTER TABLE t1 ADD COLUMN a5 $datetime DEFAULT $now FIRST;
eval ALTER TABLE t1 ADD COLUMN a4 $datetime ON UPDATE $now FIRST;

eval ALTER TABLE t1 ADD COLUMN a3 $timestamp NOT NULL DEFAULT $now ON UPDATE $now FIRST;
eval ALTER TABLE t1 ADD COLUMN a2 $timestamp NOT NULL DEFAULT $now FIRST;
eval ALTER TABLE t1 ADD COLUMN a1 $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $now FIRST;

eval ALTER TABLE t1 ADD COLUMN c1 $timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE $now AFTER b;
eval ALTER TABLE t1 ADD COLUMN c2 $timestamp NOT NULL DEFAULT $now AFTER c1;
eval ALTER TABLE t1 ADD COLUMN c3 $timestamp NOT NULL DEFAULT $now ON UPDATE $now AFTER c2;

eval ALTER TABLE t1 ADD COLUMN c4 $datetime ON UPDATE $now AFTER c3;
eval ALTER TABLE t1 ADD COLUMN c5 $datetime DEFAULT $now AFTER c4;
eval ALTER TABLE t1 ADD COLUMN c6 $datetime DEFAULT $now ON UPDATE $now AFTER c5;

SELECT * FROM t1;

DROP TABLE t1;


eval CREATE TABLE t1 ( a $timestamp  NOT NULL DEFAULT $now ON UPDATE $current_timestamp, b $datetime DEFAULT $now );
INSERT INTO t1 VALUES ();

SET timestamp = 1000000000;

ALTER TABLE t1 MODIFY COLUMN a TIMESTAMP(3) DEFAULT CURRENT_TIMESTAMP(3);
ALTER TABLE t1 MODIFY COLUMN b DATETIME(3) DEFAULT CURRENT_TIMESTAMP(3);

SELECT * FROM t1;

DROP TABLE t1;


eval CREATE TABLE t1 (
  a $timestamp NOT NULL DEFAULT '1999-12-01 11:22:33' ON UPDATE $current_timestamp,
  b $datetime DEFAULT '1999-12-01 11:22:33'
);
INSERT INTO t1 VALUES ();

eval ALTER TABLE t1 MODIFY COLUMN a $timestamp DEFAULT $now;
eval ALTER TABLE t1 MODIFY COLUMN b $datetime DEFAULT $now;
INSERT INTO t1 VALUES ();

SELECT * FROM t1;

DROP TABLE t1;
SET sql_mode = default;
