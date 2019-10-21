DROP TABLE IF EXISTS t1;
SET sql_mode='NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (c1 TINYINT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
Warnings:
Warning	1264	Out of range value for column 'c1' at row 3
Warning	1264	Out of range value for column 'c1' at row 4
Warning	1264	Out of range value for column 'c1' at row 5
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
127	abc
127	abc
127	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` tinyint DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
127	abc
127	abc
127	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` tinyint DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 SMALLINT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` smallint DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` smallint DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 MEDIUMINT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` mediumint DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` mediumint DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 INT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 INTEGER NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 BIGINT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` bigint DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` bigint DEFAULT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 TINYINT NOT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
Warnings:
Warning	1264	Out of range value for column 'c1' at row 3
Warning	1264	Out of range value for column 'c1' at row 4
Warning	1264	Out of range value for column 'c1' at row 5
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
127	abc
127	abc
127	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` tinyint NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
127	abc
127	abc
127	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` tinyint NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 SMALLINT NOT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` smallint NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` smallint NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 MEDIUMINT NOT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` mediumint NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` mediumint NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 INT NOT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 INTEGER NOT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
CREATE TABLE t1 (c1 BIGINT NOT NULL, c2 CHAR(5)) PARTITION BY HASH(c1);
INSERT INTO t1 VALUES(0,'abc'),(100,'abc'),(200,'abc'),(300,'abc'),(400,'abc');
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` bigint NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`) */
ALTER TABLE t1 REMOVE PARTITIONING;
SELECT * FROM t1 ORDER BY c1;
c1	c2
0	abc
100	abc
200	abc
300	abc
400	abc
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` bigint NOT NULL,
  `c2` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SHOW TABLES;
Tables_in_test
SET sql_mode=default;
