#------------------------------------------------------------------------
# Test case to verify table creation with only INVISIBLE columns.
#------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT INVISIBLE, f2 INT INVISIBLE);
ERROR HY000: A table must have at least one visible column.
CREATE TABLE t1 (f1 INT);
ALTER TABLE t1 ALTER f1 SET INVISIBLE;
ERROR HY000: A table must have at least one visible column.
DROP TABLE t1;
#------------------------------------------------------------------------
# Test case to verify table with INVISIBLE columns.
#------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT INVISIBLE VISIBLE);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	
f2	int	YES		NULL	
DROP TABLE t1;
CREATE TABLE t1 (f1 INT, f2 INT VISIBLE, f3 INT INVISIBLE,
f4 INT GENERATED ALWAYS AS
((`f1` + `f2`)) VIRTUAL INVISIBLE,
f5 INT GENERATED ALWAYS AS
((`f1` + `f2`)) STORED INVISIBLE);
#SHOW CREATE TABLE lists INVISIBLE columns.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL,
  `f3` int DEFAULT NULL /*!80023 INVISIBLE */,
  `f4` int GENERATED ALWAYS AS ((`f1` + `f2`)) VIRTUAL /*!80023 INVISIBLE */,
  `f5` int GENERATED ALWAYS AS ((`f1` + `f2`)) STORED /*!80023 INVISIBLE */
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
#SHOW COLUMNS FROM lists invisible columns with INVISIBLE value in EXTRA column.
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	
f2	int	YES		NULL	
f3	int	YES		NULL	INVISIBLE
f4	int	YES		NULL	VIRTUAL GENERATED INVISIBLE
f5	int	YES		NULL	STORED GENERATED INVISIBLE
# For SE hidden columns INVISIBLE value is not listed in EXTRA column.
SHOW EXTENDED COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	
f2	int	YES		NULL	
f3	int	YES		NULL	INVISIBLE
f4	int	YES		NULL	VIRTUAL GENERATED INVISIBLE
f5	int	YES		NULL	STORED GENERATED INVISIBLE
DROP TABLE t1;
CREATE TEMPORARY TABLE temp (f1 INT, f2 INT VISIBLE, f3 INT INVISIBLE,
f4 INT GENERATED ALWAYS AS
((`f1` + `f2`)) VIRTUAL INVISIBLE,
f5 INT GENERATED ALWAYS AS
((`f1` + `f2`)) STORED INVISIBLE);
SHOW CREATE TABLE temp;
Table	Create Table
temp	CREATE TEMPORARY TABLE `temp` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL,
  `f3` int DEFAULT NULL /*!80023 INVISIBLE */,
  `f4` int GENERATED ALWAYS AS ((`f1` + `f2`)) VIRTUAL /*!80023 INVISIBLE */,
  `f5` int GENERATED ALWAYS AS ((`f1` + `f2`)) STORED /*!80023 INVISIBLE */
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM temp;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	NULL
f2	int	YES		NULL	NULL
f3	int	YES		NULL	INVISIBLE
f4	int	YES		NULL	VIRTUAL GENERATED INVISIBLE
f5	int	YES		NULL	STORED GENERATED INVISIBLE
SHOW EXTENDED COLUMNS FROM temp;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	NULL
f2	int	YES		NULL	NULL
f3	int	YES		NULL	INVISIBLE
f4	int	YES		NULL	VIRTUAL GENERATED INVISIBLE
f5	int	YES		NULL	STORED GENERATED INVISIBLE
DROP TABLE temp;
#------------------------------------------------------------------------
# Test case to verify wildcard expansion in SELECT operation on table
# with INVISIBLE columns.
#------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT INVISIBLE);
INSERT INTO t1 (f1, f2) VALUES (1, 2);
SELECT * FROM t1;
f1
1
SELECT f1, f2 FROM t1;
f1	f2
1	2
SELECT *, f2 FROM t1;
f1	f2
1	2
SELECT t.f1 FROM (SELECT * FROM t1) AS t;
f1
1
SELECT t.f2 FROM (SELECT * FROM t1) AS t;
ERROR 42S22: Unknown column 't.f2' in 'field list'
CREATE TABLE t2 (f3 INT, f2 INT INVISIBLE);
INSERT INTO t2 (f3, f2) VALUES (5, 2);
SELECT t1.*, t2.f2 FROM t1 JOIN t2;
f1	f2
1	2
DROP TABLE t1, t2;
#------------------------------------------------------------------------
# Test case to verify INSERT and SELECT operations on table with
# INVISIBLE columns.
#------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT VISIBLE, f3 INT INVISIBLE);
INSERT INTO t1 VALUES (10, 20);
SELECT * FROM t1;
f1	f2
10	20
SELECT f1, f2, f3 FROM t1;
f1	f2	f3
10	20	NULL
DELETE FROM t1;
INSERT INTO t1(f1, f2, f3) VALUES (1, 2, 3);
SELECT * FROM t1;
f1	f2
1	2
SELECT f1, f2, f3 FROM t1;
f1	f2	f3
1	2	3
DELETE FROM t1;
#------------------------------------------------------------------------
# Test case to verify INVISIBLE columns with DEFAULT value.
#------------------------------------------------------------------------
ALTER TABLE t1 ALTER f3 SET DEFAULT 8;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL,
  `f3` int DEFAULT '8' /*!80023 INVISIBLE */
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1 VALUES (10, 20);
SELECT * FROM t1;
f1	f2
10	20
SELECT f1, f2, f3 FROM t1;
f1	f2	f3
10	20	8
#------------------------------------------------------------------------
# Test case to verify generated INVISIBLE columns.
#------------------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN f4 INT AS (f1 + f2) INVISIBLE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL,
  `f3` int DEFAULT '8' /*!80023 INVISIBLE */,
  `f4` int GENERATED ALWAYS AS ((`f1` + `f2`)) VIRTUAL /*!80023 INVISIBLE */
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	
f2	int	YES		NULL	
f3	int	YES		8	INVISIBLE
f4	int	YES		NULL	VIRTUAL GENERATED INVISIBLE
SELECT * FROM t1;
f1	f2
10	20
SELECT f4 FROM t1;
f4
30
DROP TABLE t1;
#------------------------------------------------------------------------
# Test case to verify INVISIBLE column attribute for all type of columns.
#------------------------------------------------------------------------
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
Table	Create Table
t1	CREATE TABLE `t1` (
  `c0` int DEFAULT NULL,
  `c1` bit(7) DEFAULT NULL /*!80023 INVISIBLE */,
  `c2` tinyint(1) DEFAULT NULL /*!80023 INVISIBLE */,
  `c3` tinyint DEFAULT NULL /*!80023 INVISIBLE */,
  `c4` smallint DEFAULT NULL /*!80023 INVISIBLE */,
  `c5` mediumint DEFAULT NULL /*!80023 INVISIBLE */,
  `c6` int DEFAULT NULL /*!80023 INVISIBLE */,
  `c7` bigint DEFAULT NULL /*!80023 INVISIBLE */,
  `c8` decimal(6,2) DEFAULT NULL /*!80023 INVISIBLE */,
  `c9` float DEFAULT NULL /*!80023 INVISIBLE */,
  `c10` double DEFAULT NULL /*!80023 INVISIBLE */,
  `c11` char(1) DEFAULT NULL /*!80023 INVISIBLE */,
  `c12` varchar(1) DEFAULT NULL /*!80023 INVISIBLE */,
  `c13` binary(1) DEFAULT NULL /*!80023 INVISIBLE */,
  `c14` varbinary(1) DEFAULT NULL /*!80023 INVISIBLE */,
  `c15` tinyblob /*!80023 INVISIBLE */,
  `c16` tinytext /*!80023 INVISIBLE */,
  `c17` blob /*!80023 INVISIBLE */,
  `c18` text /*!80023 INVISIBLE */,
  `c19` mediumblob /*!80023 INVISIBLE */,
  `c20` mediumtext /*!80023 INVISIBLE */,
  `c21` longblob /*!80023 INVISIBLE */,
  `c22` longtext /*!80023 INVISIBLE */,
  `c23` date DEFAULT NULL /*!80023 INVISIBLE */,
  `c24` datetime DEFAULT NULL /*!80023 INVISIBLE */,
  `c25` timestamp NULL DEFAULT NULL /*!80023 INVISIBLE */,
  `c26` time DEFAULT NULL /*!80023 INVISIBLE */,
  `c27` year DEFAULT NULL /*!80023 INVISIBLE */,
  `c28` json DEFAULT NULL /*!80023 INVISIBLE */,
  `c29` enum('a','b') DEFAULT NULL /*!80023 INVISIBLE */,
  `c30` set('a','b') DEFAULT NULL /*!80023 INVISIBLE */,
  `c31` point DEFAULT NULL /*!80023 INVISIBLE */,
  `c32` linestring DEFAULT NULL /*!80023 INVISIBLE */,
  `c33` multilinestring DEFAULT NULL /*!80023 INVISIBLE */,
  `c34` polygon DEFAULT NULL /*!80023 INVISIBLE */,
  `c35` multipolygon DEFAULT NULL /*!80023 INVISIBLE */
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
c0	int	YES		NULL	
c1	bit(7)	YES		NULL	INVISIBLE
c2	tinyint(1)	YES		NULL	INVISIBLE
c3	tinyint	YES		NULL	INVISIBLE
c4	smallint	YES		NULL	INVISIBLE
c5	mediumint	YES		NULL	INVISIBLE
c6	int	YES		NULL	INVISIBLE
c7	bigint	YES		NULL	INVISIBLE
c8	decimal(6,2)	YES		NULL	INVISIBLE
c9	float	YES		NULL	INVISIBLE
c10	double	YES		NULL	INVISIBLE
c11	char(1)	YES		NULL	INVISIBLE
c12	varchar(1)	YES		NULL	INVISIBLE
c13	binary(1)	YES		NULL	INVISIBLE
c14	varbinary(1)	YES		NULL	INVISIBLE
c15	tinyblob	YES		NULL	INVISIBLE
c16	tinytext	YES		NULL	INVISIBLE
c17	blob	YES		NULL	INVISIBLE
c18	text	YES		NULL	INVISIBLE
c19	mediumblob	YES		NULL	INVISIBLE
c20	mediumtext	YES		NULL	INVISIBLE
c21	longblob	YES		NULL	INVISIBLE
c22	longtext	YES		NULL	INVISIBLE
c23	date	YES		NULL	INVISIBLE
c24	datetime	YES		NULL	INVISIBLE
c25	timestamp	YES		NULL	INVISIBLE
c26	time	YES		NULL	INVISIBLE
c27	year	YES		NULL	INVISIBLE
c28	json	YES		NULL	INVISIBLE
c29	enum('a','b')	YES		NULL	INVISIBLE
c30	set('a','b')	YES		NULL	INVISIBLE
c31	point	YES		NULL	INVISIBLE
c32	linestring	YES		NULL	INVISIBLE
c33	multilinestring	YES		NULL	INVISIBLE
c34	polygon	YES		NULL	INVISIBLE
c35	multipolygon	YES		NULL	INVISIBLE
INSERT INTO t1 VALUES (10);
SELECT * FROM t1;
c0
10
DROP TABLE t1;
#------------------------------------------------------------------------
# Test case to verify INVISIBLE column update.
#------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT INVISIBLE DEFAULT 8);
INSERT INTO t1(f1, f2) VALUES  (10, 10), (20, 8);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT '8' /*!80023 INVISIBLE */
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	
f2	int	YES		8	INVISIBLE
SELECT f1, f2 FROM t1 ORDER BY f1;
f1	f2
10	10
20	8
DELETE FROM t1 WHERE f2 = 10;
UPDATE t1 SET f2 = 20;
SELECT f1, f2 FROM t1;
f1	f2
20	20
DELETE FROM t1;
#------------------------------------------------------------------------
# Test case to verify Index creation on INVISIBLE column.
#------------------------------------------------------------------------
INSERT INTO t1 VALUES  (10);
ALTER TABLE t1 ADD INDEX idx(f2);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT '8' /*!80023 INVISIBLE */,
  KEY `idx` (`f2`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	
f2	int	YES	MUL	8	INVISIBLE
#------------------------------------------------------------------------
# Test case to verify Unique Index creation on INVISIBLE column.
#------------------------------------------------------------------------
ALTER TABLE t1 DROP INDEX idx, ADD UNIQUE INDEX (f2);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT '8' /*!80023 INVISIBLE */,
  UNIQUE KEY `f2` (`f2`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1 VALUES  (10);
ERROR 23000: Duplicate entry '8' for key 't1.f2'
INSERT INTO t1(f1, f2) VALUES  (10, 10);
SELECT f1, f2 FROM t1 ORDER BY f2;
f1	f2
10	8
10	10
#------------------------------------------------------------------------
# Test case to verify Primary Key creation on INVISIBLE column.
#------------------------------------------------------------------------
ALTER TABLE t1 DROP INDEX f2, ADD PRIMARY KEY (f2);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int NOT NULL DEFAULT '8' /*!80023 INVISIBLE */,
  PRIMARY KEY (`f2`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1 VALUES  (10);
ERROR 23000: Duplicate entry '8' for key 't1.PRIMARY'
INSERT INTO t1(f1, f2) VALUES  (10, 20);
SELECT f1, f2 FROM t1 ORDER BY f2;
f1	f2
10	8
10	10
10	20
#------------------------------------------------------------------------
# Test case to verify INVISIBLE auto_increment column.
#------------------------------------------------------------------------
ALTER TABLE t1 MODIFY f2 INT INVISIBLE AUTO_INCREMENT;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int NOT NULL AUTO_INCREMENT /*!80023 INVISIBLE */,
  PRIMARY KEY (`f2`)
) ENGINE=MyISAM AUTO_INCREMENT=21 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1 VALUES  (10);
SELECT f1, f2 FROM t1 ORDER BY f2;
f1	f2
10	8
10	10
10	20
10	21
#------------------------------------------------------------------------
# Test case to verify Check Constraint on INVISIBLE column.
#------------------------------------------------------------------------
ALTER TABLE t1 MODIFY f2 INT INVISIBLE, ADD CHECK(f2 < 20000);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int NOT NULL /*!80023 INVISIBLE */,
  PRIMARY KEY (`f2`),
  CONSTRAINT `t1_chk_1` CHECK ((`f2` < 20000))
) ENGINE=MyISAM AUTO_INCREMENT=22 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1(f1, f2) VALUES  (10, 20000);
ERROR HY000: Check constraint 't1_chk_1' is violated.
SELECT f1, f2 FROM t1 ORDER BY f2;
f1	f2
10	8
10	10
10	20
10	21
DROP TABLE t1;
#------------------------------------------------------------------------
# Test case to verify column visibility alter using ALTER clause of
# ALTER TABLE statement.
#------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT);
INSERT INTO t1 VALUES (10, 90);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
f1	f2
10	90
ALTER TABLE t1 ALTER COLUMN f1 SET INVISIBLE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL /*!80023 INVISIBLE */,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
f2
90
SELECT f1, f2 FROM t1;
f1	f2
10	90
ALTER TABLE t1 ALTER COLUMN f2 SET INVISIBLE;
ERROR HY000: A table must have at least one visible column.
ALTER TABLE t1 ALTER COLUMN f1 SET VISIBLE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
f1	f2
10	90
#------------------------------------------------------------------------
# Test case to verify column visibility alter using CHANGE clause of
# ALTER TABLE statement.
#------------------------------------------------------------------------
ALTER TABLE t1 CHANGE f1 f1 INT INVISIBLE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL /*!80023 INVISIBLE */,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
f2
90
SELECT f1, f2 FROM t1;
f1	f2
10	90
ALTER TABLE t1 CHANGE f2 f2 INT INVISIBLE;
ERROR HY000: A table must have at least one visible column.
ALTER TABLE t1 CHANGE f1 f1 INT VISIBLE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
f1	f2
10	90
#------------------------------------------------------------------------
# Test case to verify column visibility alter using MODIFY clause of
# ALTER TABLE statement.
#------------------------------------------------------------------------
ALTER TABLE t1 MODIFY f1 INT INVISIBLE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL /*!80023 INVISIBLE */,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
f2
90
SELECT f1, f2 FROM t1;
f1	f2
10	90
ALTER TABLE t1 MODIFY f2 INT INVISIBLE;
ERROR HY000: A table must have at least one visible column.
ALTER TABLE t1 MODIFY f1 INT VISIBLE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
f1	f2
10	90
DROP TABLE t1;
#------------------------------------------------------------------------
# Test case to verify LOAD DATA.
#------------------------------------------------------------------------
CREATE TABLE t1(f1 INT INVISIBLE, f2 INT);
CREATE TABLE t2(f1 INT, f2 INT);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL /*!80023 INVISIBLE */,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1(f1, f2) VALUES (10, 20), (20, 30);
INSERT INTO t2 VALUES (10, 20), (20, 30);
SELECT * FROM t1 INTO OUTFILE 'MYSQLTEST_VARDIR/tmp/test1.sql';;
DELETE FROM t1;
LOAD DATA INFILE 'MYSQLTEST_VARDIR/tmp/test1.sql' INTO TABLE t1;;
SELECT * FROM t1 ORDER BY f2;
f2
20
30
SELECT f1, f2 FROM t1 ORDER BY f2;
f1	f2
NULL	20
NULL	30
DELETE FROM t1;
LOAD DATA INFILE 'MYSQLTEST_VARDIR/tmp/test1.sql' INTO TABLE t1 (f1) SET f2 = 10;;
SELECT * FROM t1 ORDER BY f2;
f2
10
10
SELECT f1, f2 FROM t1 ORDER BY f1;
f1	f2
20	10
30	10
#------------------------------------------------------------------------
# Test case to verify LOAD DATA from fixed length row.
#------------------------------------------------------------------------
SELECT * FROM t1 INTO OUTFILE 'MYSQLTEST_VARDIR/tmp/test2.sql' FIELDS TERMINATED BY '' ENCLOSED BY '';;
DELETE FROM t1;
LOAD DATA INFILE 'MYSQLTEST_VARDIR/tmp/test2.sql' INTO TABLE t1 FIELDS TERMINATED BY '' ENCLOSED BY '';;
SELECT * FROM t1;
f2
10
10
SELECT f1, f2 FROM t1;
f1	f2
NULL	10
NULL	10
DELETE FROM t1;
LOAD DATA INFILE 'MYSQLTEST_VARDIR/tmp/test2.sql' INTO TABLE t1 FIELDS TERMINATED BY '' ENCLOSED BY '' (f1) SET f2 = f1 + 10;;
SELECT * FROM t1;
f2
20
20
SELECT f1, f2 FROM t1;
f1	f2
10	20
10	20
#------------------------------------------------------------------------
# Test case to verify LOAD DATA in XML format.
#------------------------------------------------------------------------
DELETE FROM t1;
LOAD XML INFILE "MYSQLTEST_VARDIR/tmp/tmp1.xml" INTO TABLE t1 (f1, f2);;
SELECT * FROM t1;
f2
20
20
SELECT f1, f2 FROM t1;
f1	f2
10	20
10	20
ALTER TABLE t1 DROP COLUMN f1;
DELETE FROM t1;
ALTER TABLE t1 ADD COLUMN f1 INT INVISIBLE;
LOAD XML INFILE "MYSQLTEST_VARDIR/tmp/tmp1.xml" INTO TABLE t1;;
SELECT * FROM t1;
f2
20
20
SELECT f1, f2 FROM t1;
f1	f2
NULL	20
NULL	20
DROP TABLE t1, t2;
#------------------------------------------------------------------------
# Test case to verify view on INVISIBLE column.
#------------------------------------------------------------------------
CREATE TABLE t1(f1 INT INVISIBLE, f2 INT);
INSERT INTO t1(f1, f2) VALUES (10, 20), (30, 40);
# View is created with only f2.
CREATE VIEW v1 AS SELECT * FROM t1;
SHOW CREATE VIEW v1;
View	Create View	character_set_client	collation_connection
v1	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v1` AS select `t1`.`f2` AS `f2` from `t1`	utf8mb4	utf8mb4_0900_ai_ci
SHOW COLUMNS FROM v1;
Field	Type	Null	Key	Default	Extra
f2	int	YES		NULL	
SELECT * FROM v1 ORDER BY f2;
f2
20
40
# View is created with f1 and f2;
CREATE VIEW v2 AS SELECT f1, f2 FROM t1;
SHOW CREATE VIEW v2;
View	Create View	character_set_client	collation_connection
v2	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v2` AS select `t1`.`f1` AS `f1`,`t1`.`f2` AS `f2` from `t1`	utf8mb4	utf8mb4_0900_ai_ci
# Both columns are listed as VISIBLE.
SHOW COLUMNS FROM v2;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	
f2	int	YES		NULL	
SELECT * FROM v2 ORDER BY f2;
f1	f2
10	20
30	40
# View is created with only f1;
CREATE VIEW v3 AS SELECT f1 FROM t1;
SHOW CREATE VIEW v3;
View	Create View	character_set_client	collation_connection
v3	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v3` AS select `t1`.`f1` AS `f1` from `t1`	utf8mb4	utf8mb4_0900_ai_ci
# Column f1 is listed as VISIBLE.
SHOW COLUMNS FROM v3;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	
SELECT * FROM v3 ORDER BY f1;
f1
10
30
DROP VIEW v1, v2;
#------------------------------------------------------------------------
# Test case to verify INFORMATION_SCHEMA.COLUMNS result set.
#------------------------------------------------------------------------
SELECT TABLE_NAME, COLUMN_NAME, EXTRA FROM INFORMATION_SCHEMA.COLUMNS
WHERE TABLE_SCHEMA='test' ORDER BY TABLE_NAME, COLUMN_NAME;
TABLE_NAME	COLUMN_NAME	EXTRA
t1	f1	INVISIBLE
t1	f2	
v3	f1	
DROP VIEW v3;
#------------------------------------------------------------------------
# Test case to verify REPLACE operation with INVISIBLE column.
#------------------------------------------------------------------------
ALTER TABLE t1 ADD PRIMARY KEY (f1);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL /*!80023 INVISIBLE */,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT f1, f2 FROM t1 ORDER BY f2;
f1	f2
10	20
30	40
REPLACE INTO t1(f1, f2) VALUE (10, 88);
SELECT f1, f2 FROM t1 ORDER BY f2;
f1	f2
30	40
10	88
#------------------------------------------------------------------------
# Test case to verify INSERT ON DUPLICATE KEY UPDATE operation with
# INVISIBLE column.
#------------------------------------------------------------------------
INSERT INTO t1(f1, f2) VALUES (30, 44) ON DUPLICATE KEY UPDATE f2=88;
SELECT f1, f2 FROM t1 ORDER BY f1;
f1	f2
10	88
30	88
#------------------------------------------------------------------------
# Test case to verify INVISIBLE columns with CREATE TABLE LIKE
#------------------------------------------------------------------------
CREATE TABLE t2 LIKE t1;
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int NOT NULL /*!80023 INVISIBLE */,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t2;
Field	Type	Null	Key	Default	Extra
f1	int	NO	PRI	NULL	INVISIBLE
f2	int	YES		NULL	
DROP TABLE t2;
#------------------------------------------------------------------------
# Test case to verify INVISIBLE columns with CREATE TABLE ... SELECT
#------------------------------------------------------------------------
CREATE TABLE t2 SELECT * FROM t1;
# Table is created with only f2 column.
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t2;
Field	Type	Null	Key	Default	Extra
f2	int	YES		NULL	
SELECT * FROM t2;
f2
88
88
DROP TABLE t2;
CREATE TABLE t2 SELECT f1, f2 FROM t1;
# Both f1 and f2 are created as VISIBLE columns.
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t2;
Field	Type	Null	Key	Default	Extra
f1	int	NO		NULL	
f2	int	YES		NULL	
SELECT * FROM t2 ORDER BY f1;
f1	f2
10	88
30	88
DROP TABLE t2;
CREATE TABLE t2(f1 INT INVISIBLE) SELECT f1, f2 FROM t1;
# f1 is created as INVISIBLE columns and f2 as VISIBLE columns.
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL /*!80023 INVISIBLE */,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t2;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	INVISIBLE
f2	int	YES		NULL	
SELECT * FROM t2;
f2
88
88
DROP TABLE t2;
CREATE TEMPORARY TABLE t2 SELECT * FROM t1;
# Table is created with only f2 column.
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TEMPORARY TABLE `t2` (
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t2;
Field	Type	Null	Key	Default	Extra
f2	int	YES		NULL	NULL
SELECT * FROM t2;
f2
88
88
DROP TABLE t2;
CREATE TEMPORARY TABLE t2 SELECT f1, f2 FROM t1;
# Both f1 and f2 are created as VISIBLE columns.
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TEMPORARY TABLE `t2` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t2;
Field	Type	Null	Key	Default	Extra
f1	int	NO		NULL	NULL
f2	int	YES		NULL	NULL
SELECT * FROM t2 ORDER BY f1;
f1	f2
10	88
30	88
DROP TABLE t2;
CREATE TEMPORARY TABLE t2(f1 INT INVISIBLE) SELECT f1, f2 FROM t1;
# f1 is created as INVISIBLE columns and f2 as VISIBLE columns.
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TEMPORARY TABLE `t2` (
  `f1` int DEFAULT NULL /*!80023 INVISIBLE */,
  `f2` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW COLUMNS FROM t2;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	INVISIBLE
f2	int	YES		NULL	NULL
SELECT * FROM t2;
f2
88
88
DROP TABLE t2;
DROP TABLE t1;
#------------------------------------------------------------------------
# Test case to verify table and column privileges on INVISIBLE columns.
#------------------------------------------------------------------------
CREATE USER user1@localhost;
CREATE DATABASE db1;
USE db1;
CREATE TABLE t1(f1 INT DEFAULT 10, f2 INT);
INSERT INTO t1(f2) VALUES(1);
GRANT SELECT(f2) ON db1.t1 TO user1@localhost;
# user1 has SELECT privilege on only column f2;
connect  con1, localhost, user1, , db1;
SELECT * FROM t1;
ERROR 42000: SELECT command denied to user 'user1'@'localhost' for column 'f1' in table 't1'
# Make t1.f1 invisible.
connection default;
ALTER TABLE t1 ALTER COLUMN f1 SET INVISIBLE;
# 'SELECT *' works as only column f2 is listed.
connection con1;
SELECT * FROM t1;
f2
1
SELECT f1 FROM t1;
ERROR 42000: SELECT command denied to user 'user1'@'localhost' for column 'f1' in table 't1'
connection default;
REVOKE ALL ON db1.t1 FROM user1@localhost;
# user1 does not have any privilges on t1 table.
connection con1;
INSERT INTO t1 VALUES(2);
ERROR 42000: INSERT command denied to user 'user1'@'localhost' for table 't1'
INSERT INTO t1(f1) VALUES (11);
ERROR 42000: INSERT command denied to user 'user1'@'localhost' for table 't1'
# Table privilege INSERT is granted to user1.
connection default;
GRANT INSERT ON db1.t1 TO user1@localhost;
connection con1;
INSERT INTO t1 VALUES(2);
INSERT INTO t1(f1) VALUES (11);
# ALL privileges are REVOKED from user1.
connection default;
REVOKE ALL ON db1.t1 FROM user1@localhost;
connection con1;
INSERT INTO t1 VALUES(3);
ERROR 42000: INSERT command denied to user 'user1'@'localhost' for table 't1'
INSERT INTO t1(f1) VALUES (12);
ERROR 42000: INSERT command denied to user 'user1'@'localhost' for table 't1'
# INSERT on t1.f1 column and SELECT on t1.f1, t1.f2 columns are granted to user1
connection default;
GRANT INSERT(f1), SELECT(f1, f2) ON db1.t1 TO user1@localhost;
connection con1;
INSERT INTO t1 VALUES(3);
ERROR 42000: INSERT command denied to user 'user1'@'localhost' for table 't1'
INSERT INTO t1(f1) VALUES (12);
connection default;
disconnect con1;
USE test;
DROP DATABASE db1;
DROP USER user1@localhost;
#------------------------------------------------------------------------
# Test case to verify natural join with invisible columns.
#------------------------------------------------------------------------
CREATE TABLE t1(a INT, b INT INVISIBLE);
CREATE TABLE t2(c INT, b INT INVISIBLE);
INSERT INTO t1(b) VALUES (1), (2), (3);
INSERT INTO t2(b) VALUES (1), (2), (3);
SELECT * FROM t1 JOIN t2;
a	c
NULL	NULL
NULL	NULL
NULL	NULL
NULL	NULL
NULL	NULL
NULL	NULL
NULL	NULL
NULL	NULL
NULL	NULL
SELECT * FROM t1 JOIN t2 WHERE (t1.b = t2.b);
a	c
NULL	NULL
NULL	NULL
NULL	NULL
SELECT * FROM t1 JOIN t2 ON (t1.b = t2.b);
a	c
NULL	NULL
NULL	NULL
NULL	NULL
SELECT * FROM t1 JOIN t2 USING(b) ORDER BY (b);
b	a	c
1	NULL	NULL
2	NULL	NULL
3	NULL	NULL
DROP TABLE t1, t2;
#------------------------------------------------------------------------
# Test case to verify my_row_id name usage for invisible columns.
#------------------------------------------------------------------------
CREATE TABLE t1(a INT, my_row_id INT);
CREATE TABLE t2(a INT, my_row_id INT INVISIBLE);
DROP TABLE t1, t2;
