CALL mtr.add_suppression('Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT.');
CREATE TABLE tr (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id));
INSERT INTO tr VALUES
(1, 'desk organiser', '2003-10-15'),
(2, 'CD player', '1993-11-05'),
(3, 'TV set', '1996-03-10'),
(4, 'bookcase', '1982-01-10'),
(5, 'exercise bike', '2004-05-09'),
(6, 'sofa', '1987-06-05');
# Test for ADD PARTITION
CREATE TABLE t1 (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id))
PARTITION BY RANGE( YEAR(purchased) ) (
PARTITION p0 VALUES LESS THAN (1990),
PARTITION p1 VALUES LESS THAN (1995),
PARTITION p2 VALUES LESS THAN (2000),
PARTITION p3 VALUES LESS THAN (2005)
);
INSERT INTO t1 SELECT * FROM tr;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
DELETE FROM t1 WHERE id = 6;
SELECT MAX(id) AS `Expect 5` FROM t1;
Expect 5
5
ALTER TABLE t1 ALGORITHM = INPLACE, ADD PARTITION (PARTITION p4 VALUES LESS THAN (2010), PARTITION p5 VALUES LESS THAN (2015));
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p2.ibd
./test/t1#p#p3.ibd
./test/t1#p#p4.ibd
./test/t1#p#p5.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB,
 PARTITION p4 VALUES LESS THAN (2010) ENGINE = InnoDB,
 PARTITION p5 VALUES LESS THAN (2015) ENGINE = InnoDB) */
INSERT INTO t1 VALUES (0, 'popcorn maker', '2010-11-22');
SELECT MAX(id) AS `Expect 7` FROM t1;
Expect 7
7
ALTER TABLE t1 ALGORITHM = INPLACE, ADD PARTITION (PARTITION p6 VALUES LESS THAN (2018), PARTITION p7 VALUES LESS THAN (2020));
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p2.ibd
./test/t1#p#p3.ibd
./test/t1#p#p4.ibd
./test/t1#p#p5.ibd
./test/t1#p#p6.ibd
./test/t1#p#p7.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB,
 PARTITION p4 VALUES LESS THAN (2010) ENGINE = InnoDB,
 PARTITION p5 VALUES LESS THAN (2015) ENGINE = InnoDB,
 PARTITION p6 VALUES LESS THAN (2018) ENGINE = InnoDB,
 PARTITION p7 VALUES LESS THAN (2020) ENGINE = InnoDB) */
# restart
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB,
 PARTITION p4 VALUES LESS THAN (2010) ENGINE = InnoDB,
 PARTITION p5 VALUES LESS THAN (2015) ENGINE = InnoDB,
 PARTITION p6 VALUES LESS THAN (2018) ENGINE = InnoDB,
 PARTITION p7 VALUES LESS THAN (2020) ENGINE = InnoDB) */
INSERT INTO t1 VALUES
(0, 'aquarium', '2019-08-04'),
(0, 'study desk', '2016-06-18');
SELECT MAX(id) AS `Expect 9` FROM t1;
Expect 9
9
DROP TABLE t1;
# Test for DROP PARTITION, along with some ADD PARTITION
CREATE TABLE t1 (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id))
PARTITION BY RANGE( YEAR(purchased) ) (
PARTITION p0 VALUES LESS THAN (1990),
PARTITION p1 VALUES LESS THAN (1995),
PARTITION p2 VALUES LESS THAN (2000),
PARTITION p3 VALUES LESS THAN (2005)
);
INSERT INTO t1 SELECT * FROM tr;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
SELECT id FROM t1 PARTITION(p0);
id
4
6
SELECT id FROM t1 PARTITION(p1);
id
2
SELECT id FROM t1 PARTITION(p2);
id
3
SELECT id FROM t1 PARTITION(p3);
id
1
5
# This doesn't delete the row containing max(id) in the table
ALTER TABLE t1 ALGORITHM = INPLACE, DROP PARTITION p2;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p3.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
INSERT INTO t1 VALUES (0, 'aquarium', '2001-07-02');
SELECT MAX(id) AS `Expect 7` FROM t1;
Expect 7
7
# This will delete the row containing max(id) in the table
ALTER TABLE t1 ALGORITHM = INPLACE, DROP PARTITION p3;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/tr.ibd
SELECT MAX(id) AS `Expect 6` FROM t1;
Expect 6
6
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB) */
INSERT INTO t1 VALUES(0, 'study desk', '1988-09-16');
SELECT MAX(id) AS `Expect 8` FROM t1;
Expect 8
8
ALTER TABLE t1 ALGORITHM = INPLACE, ADD PARTITION (PARTITION p3 VALUES LESS THAN (2008));
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p3.ibd
./test/tr.ibd
ALTER TABLE t1 ALGORITHM = INPLACE, DROP PARTITION p0;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p1.ibd
./test/t1#p#p3.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2008) ENGINE = InnoDB) */
# restart
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2008) ENGINE = InnoDB) */
INSERT INTO t1 VALUES(0, 'lava lamp', '2006-01-02');
SELECT MAX(id) AS `Expect 9` FROM t1;
Expect 9
9
DELETE FROM t1 WHERE id >= 0;
ALTER TABLE t1 ALGORITHM = INPLACE, DROP PARTITION p3;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p1.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB) */
INSERT INTO t1 VALUES(0, 'lava lamp', '1992-01-15');
SELECT MAX(id) AS `Expect 10` FROM t1;
Expect 10
10
DROP TABLE t1;
# Test for ADD PARTITION for HASH PARTITIONING
CREATE TABLE t1 (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id))
PARTITION BY HASH( YEAR(purchased) ) PARTITIONS 4;
INSERT INTO t1 SELECT * FROM tr;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 4 */
DELETE FROM t1 WHERE id = 6;
SELECT MAX(id) AS `Expect 5` FROM t1;
Expect 5
5
ALTER TABLE t1 ALGORITHM = INPLACE, ADD PARTITION PARTITIONS 2;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p2.ibd
./test/t1#p#p3.ibd
./test/t1#p#p4.ibd
./test/t1#p#p5.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 6 */
INSERT INTO t1 VALUES (0, 'popcorn maker', '2010-11-22');
SELECT MAX(id) AS `Expect 7` FROM t1;
Expect 7
7
ALTER TABLE t1 ALGORITHM = INPLACE, ADD PARTITION PARTITIONS 2;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p2.ibd
./test/t1#p#p3.ibd
./test/t1#p#p4.ibd
./test/t1#p#p5.ibd
./test/t1#p#p6.ibd
./test/t1#p#p7.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 8 */
# restart
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 8 */
INSERT INTO t1 VALUES
(0, 'aquarium', '2019-08-04'),
(0, 'study desk', '2016-06-18');
SELECT MAX(id) AS `Expect 9` FROM t1;
Expect 9
9
DROP TABLE t1;
# Test for ADD PARTITION for HASH PARTITIONINNG,
# along with COALESCE PARTITION
CREATE TABLE t1 (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id))
PARTITION BY HASH( YEAR(purchased) ) PARTITIONS 8;
INSERT INTO t1 SELECT * FROM tr;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 8 */
DELETE FROM t1 WHERE id = 6;
SELECT MAX(id) AS `Expect 5` FROM t1;
Expect 5
5
ALTER TABLE t1 ALGORITHM = INPLACE, COALESCE PARTITION 2;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p2.ibd
./test/t1#p#p3.ibd
./test/t1#p#p4.ibd
./test/t1#p#p5.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 6 */
INSERT INTO t1 VALUES (0, 'popcorn maker', '2010-11-22');
SELECT MAX(id) AS `Expect 7` FROM t1;
Expect 7
7
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 6 */
ALTER TABLE t1 ALGORITHM = INPLACE, COALESCE PARTITION 4;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/tr.ibd
# restart
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 2 */
INSERT INTO t1 VALUES
(0, 'aquarium', '2019-08-04'),
(0, 'study desk', '2016-06-18');
SELECT MAX(id) AS `Expect 9` FROM t1;
Expect 9
9
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (year(`purchased`))
PARTITIONS 2 */
DROP TABLE t1;
# Test for REORGANIZE PARTITION
CREATE TABLE t1 (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id))
PARTITION BY RANGE( YEAR(purchased) ) (
PARTITION p0 VALUES LESS THAN (1990),
PARTITION p1 VALUES LESS THAN (1995),
PARTITION p2 VALUES LESS THAN (2000),
PARTITION p3 VALUES LESS THAN (2005)
);
INSERT INTO t1 SELECT * FROM tr;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
DELETE FROM t1 WHERE id = 6;
SELECT MAX(id) AS `Expect 5` FROM t1;
Expect 5
5
ALTER TABLE t1 ALGORITHM = INPLACE, REORGANIZE PARTITION p0, p1 INTO
(PARTITION p01 VALUES LESS THAN(1985), PARTITION p02 VALUES LESS THAN(1990),
PARTITION p11 VALUES LESS THAN(1993), PARTITION p12 VALUES LESS THAN(1995));
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p01.ibd
./test/t1#p#p02.ibd
./test/t1#p#p11.ibd
./test/t1#p#p12.ibd
./test/t1#p#p2.ibd
./test/t1#p#p3.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p01 VALUES LESS THAN (1985) ENGINE = InnoDB,
 PARTITION p02 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p11 VALUES LESS THAN (1993) ENGINE = InnoDB,
 PARTITION p12 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
INSERT INTO t1 VALUES (0, 'popcorn maker', '2001-11-22');
SELECT MAX(id) AS `Expect 7` FROM t1;
Expect 7
7
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p01 VALUES LESS THAN (1985) ENGINE = InnoDB,
 PARTITION p02 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p11 VALUES LESS THAN (1993) ENGINE = InnoDB,
 PARTITION p12 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
ALTER TABLE t1 ALGORITHM = INPLACE, REORGANIZE PARTITION p2, p3 INTO
(PARTITION p21 VALUES LESS THAN(1997), PARTITION p22 VALUES LESS THAN(2000),
PARTITION p31 VALUES LESS THAN(2002), PARTITION p32 VALUES LESS THAN(2005));
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p01.ibd
./test/t1#p#p02.ibd
./test/t1#p#p11.ibd
./test/t1#p#p12.ibd
./test/t1#p#p21.ibd
./test/t1#p#p22.ibd
./test/t1#p#p31.ibd
./test/t1#p#p32.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p01 VALUES LESS THAN (1985) ENGINE = InnoDB,
 PARTITION p02 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p11 VALUES LESS THAN (1993) ENGINE = InnoDB,
 PARTITION p12 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p21 VALUES LESS THAN (1997) ENGINE = InnoDB,
 PARTITION p22 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p31 VALUES LESS THAN (2002) ENGINE = InnoDB,
 PARTITION p32 VALUES LESS THAN (2005) ENGINE = InnoDB) */
INSERT INTO t1 VALUES
(0, 'aquarium', '2002-08-04'),
(0, 'study desk', '2004-06-18');
SELECT MAX(id) AS `Expect 9` FROM t1;
Expect 9
9
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p01 VALUES LESS THAN (1985) ENGINE = InnoDB,
 PARTITION p02 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p11 VALUES LESS THAN (1993) ENGINE = InnoDB,
 PARTITION p12 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p21 VALUES LESS THAN (1997) ENGINE = InnoDB,
 PARTITION p22 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p31 VALUES LESS THAN (2002) ENGINE = InnoDB,
 PARTITION p32 VALUES LESS THAN (2005) ENGINE = InnoDB) */
# Test for REBUILD PARTITION
ALTER TABLE t1 ALGORITHM = INPLACE, REBUILD PARTITION p01, p02, p31, p32;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p01.ibd
./test/t1#p#p02.ibd
./test/t1#p#p11.ibd
./test/t1#p#p12.ibd
./test/t1#p#p21.ibd
./test/t1#p#p22.ibd
./test/t1#p#p31.ibd
./test/t1#p#p32.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p01 VALUES LESS THAN (1985) ENGINE = InnoDB,
 PARTITION p02 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p11 VALUES LESS THAN (1993) ENGINE = InnoDB,
 PARTITION p12 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p21 VALUES LESS THAN (1997) ENGINE = InnoDB,
 PARTITION p22 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p31 VALUES LESS THAN (2002) ENGINE = InnoDB,
 PARTITION p32 VALUES LESS THAN (2005) ENGINE = InnoDB) */
INSERT INTO t1 VALUES (0, 'lava lamp', '2000-12-10');
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=11 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p01 VALUES LESS THAN (1985) ENGINE = InnoDB,
 PARTITION p02 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p11 VALUES LESS THAN (1993) ENGINE = InnoDB,
 PARTITION p12 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p21 VALUES LESS THAN (1997) ENGINE = InnoDB,
 PARTITION p22 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p31 VALUES LESS THAN (2002) ENGINE = InnoDB,
 PARTITION p32 VALUES LESS THAN (2005) ENGINE = InnoDB) */
SELECT MAX(id) AS `Expect 10` FROM t1;
Expect 10
10
DROP TABLE t1;
# Test for EXCHANGE PARTITION
CREATE TABLE t1 (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id))
PARTITION BY RANGE( YEAR(purchased) ) (
PARTITION p0 VALUES LESS THAN (1990),
PARTITION p1 VALUES LESS THAN (1995),
PARTITION p2 VALUES LESS THAN (2000),
PARTITION p3 VALUES LESS THAN (2005)
);
INSERT INTO t1 SELECT * FROM tr;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
DELETE FROM t1 WHERE id = 6;
SELECT MAX(id) AS `Expect 5` FROM t1;
Expect 5
5
CREATE TABLE tp (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id)) ENGINE = InnoDB;
INSERT INTO tp VALUES (100, 'DVD player', '2002-03-21');
ALTER TABLE t1 EXCHANGE PARTITION p3 WITH TABLE tp;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p2.ibd
./test/t1#p#p3.ibd
./test/tp.ibd
./test/tr.ibd
SHOW CREATE TABLE tp;
Table	Create Table
tp	CREATE TABLE `tp` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=101 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=101 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
INSERT INTO tp VALUES(0, 'DVD player', '2002-03-22');
INSERT INTO t1 VALUES(0, 'DVD player', '2002-03-23');
SELECT MAX(id) AS `Expect 101` FROM t1;
Expect 101
101
SELECT MAX(id) AS `Expect 101` FROM tp;
Expect 101
101
ALTER TABLE t1 EXCHANGE PARTITION p3 WITH TABLE tp;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0.ibd
./test/t1#p#p1.ibd
./test/t1#p#p2.ibd
./test/t1#p#p3.ibd
./test/tp.ibd
./test/tr.ibd
SHOW CREATE TABLE tp;
Table	Create Table
tp	CREATE TABLE `tp` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=102 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=102 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990) ENGINE = InnoDB,
 PARTITION p1 VALUES LESS THAN (1995) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (2000) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (2005) ENGINE = InnoDB) */
INSERT INTO tp VALUES(0, 'DVD player', '2002-03-24');
INSERT INTO t1 VALUES(0, 'DVD player', '2002-03-25');
SELECT MAX(id) AS `Expect 102` FROM t1;
Expect 102
102
SELECT MAX(id) AS `Expect 102` FROM tp;
Expect 102
102
DROP TABLE tp, t1;
# Test for subpartitioning
CREATE TABLE t1 (
id INT NOT NULL AUTO_INCREMENT,
name VARCHAR(50),
purchased DATE,
KEY(id))
PARTITION BY RANGE( YEAR(purchased) )
SUBPARTITION BY HASH ( YEAR(purchased) )
(PARTITION p0 VALUES LESS THAN (1990)
(SUBPARTITION sp00, SUBPARTITION sp01),
PARTITION p1 VALUES LESS THAN (2000)
(SUBPARTITION sp10, SUBPARTITION sp11),
PARTITION p2 VALUES LESS THAN (2010)
(SUBPARTITION sp20, SUBPARTITION sp21));
INSERT INTO t1 SELECT * FROM tr;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990)
 (SUBPARTITION sp00 ENGINE = InnoDB,
  SUBPARTITION sp01 ENGINE = InnoDB),
 PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (2010)
 (SUBPARTITION sp20 ENGINE = InnoDB,
  SUBPARTITION sp21 ENGINE = InnoDB)) */
DELETE FROM t1 WHERE id = 6;
SELECT MAX(id) AS `Expect 5` FROM t1;
Expect 5
5
INSERT INTO t1 VALUES
(0, 'popcorn maker', '2008-07-05'),
(0, 'lava lamp', '2009-01-23');
SELECT MAX(id) AS `Expect 8` FROM t1;
Expect 8
8
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990)
 (SUBPARTITION sp00 ENGINE = InnoDB,
  SUBPARTITION sp01 ENGINE = InnoDB),
 PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (2010)
 (SUBPARTITION sp20 ENGINE = InnoDB,
  SUBPARTITION sp21 ENGINE = InnoDB)) */
ALTER TABLE t1 ADD PARTITION (PARTITION p3 VALUES LESS THAN(2015) (SUBPARTITION sp30, SUBPARTITION sp31));
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0#sp#sp00.ibd
./test/t1#p#p0#sp#sp01.ibd
./test/t1#p#p1#sp#sp10.ibd
./test/t1#p#p1#sp#sp11.ibd
./test/t1#p#p2#sp#sp20.ibd
./test/t1#p#p2#sp#sp21.ibd
./test/t1#p#p3#sp#sp30.ibd
./test/t1#p#p3#sp#sp31.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990)
 (SUBPARTITION sp00 ENGINE = InnoDB,
  SUBPARTITION sp01 ENGINE = InnoDB),
 PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (2010)
 (SUBPARTITION sp20 ENGINE = InnoDB,
  SUBPARTITION sp21 ENGINE = InnoDB),
 PARTITION p3 VALUES LESS THAN (2015)
 (SUBPARTITION sp30 ENGINE = InnoDB,
  SUBPARTITION sp31 ENGINE = InnoDB)) */
INSERT INTO t1 VALUES
(0, 'study desk', '2012-02-20');
SELECT MAX(id) AS `Expect 9` FROM t1;
Expect 9
9
DELETE FROM t1 WHERE id = 9;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990)
 (SUBPARTITION sp00 ENGINE = InnoDB,
  SUBPARTITION sp01 ENGINE = InnoDB),
 PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (2010)
 (SUBPARTITION sp20 ENGINE = InnoDB,
  SUBPARTITION sp21 ENGINE = InnoDB),
 PARTITION p3 VALUES LESS THAN (2015)
 (SUBPARTITION sp30 ENGINE = InnoDB,
  SUBPARTITION sp31 ENGINE = InnoDB)) */
ALTER TABLE t1 DROP PARTITION p3;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0#sp#sp00.ibd
./test/t1#p#p0#sp#sp01.ibd
./test/t1#p#p1#sp#sp10.ibd
./test/t1#p#p1#sp#sp11.ibd
./test/t1#p#p2#sp#sp20.ibd
./test/t1#p#p2#sp#sp21.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990)
 (SUBPARTITION sp00 ENGINE = InnoDB,
  SUBPARTITION sp01 ENGINE = InnoDB),
 PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (2010)
 (SUBPARTITION sp20 ENGINE = InnoDB,
  SUBPARTITION sp21 ENGINE = InnoDB)) */
INSERT INTO t1 VALUES
(0, 'study desk', '2009-05-06');
SELECT MAX(id) AS `Expect 10` FROM t1;
Expect 10
10
INSERT INTO t1 VALUES
(0, 'lava lamp', '2007-12-25'),
(0, 'lava lamp', '2007-12-26'),
(0, 'lava lamp', '2003-12-27');
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=14 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990)
 (SUBPARTITION sp00 ENGINE = InnoDB,
  SUBPARTITION sp01 ENGINE = InnoDB),
 PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (2010)
 (SUBPARTITION sp20 ENGINE = InnoDB,
  SUBPARTITION sp21 ENGINE = InnoDB)) */
ALTER TABLE t1 REORGANIZE PARTITION p2 INTO
(PARTITION p21 VALUES LESS THAN (2005)
(SUBPARTITION sp210, SUBPARTITION sp211),
PARTITION p22 VALUES LESS THAN (2010)
(SUBPARTITION sp220, SUBPARTITION sp221));
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p0#sp#sp00.ibd
./test/t1#p#p0#sp#sp01.ibd
./test/t1#p#p1#sp#sp10.ibd
./test/t1#p#p1#sp#sp11.ibd
./test/t1#p#p21#sp#sp210.ibd
./test/t1#p#p21#sp#sp211.ibd
./test/t1#p#p22#sp#sp220.ibd
./test/t1#p#p22#sp#sp221.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=14 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p0 VALUES LESS THAN (1990)
 (SUBPARTITION sp00 ENGINE = InnoDB,
  SUBPARTITION sp01 ENGINE = InnoDB),
 PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p21 VALUES LESS THAN (2005)
 (SUBPARTITION sp210 ENGINE = InnoDB,
  SUBPARTITION sp211 ENGINE = InnoDB),
 PARTITION p22 VALUES LESS THAN (2010)
 (SUBPARTITION sp220 ENGINE = InnoDB,
  SUBPARTITION sp221 ENGINE = InnoDB)) */
INSERT INTO t1 VALUES(0, 'lava lamp', '2002-12-28');
SELECT MAX(id) AS `Expect 14` FROM t1;
Expect 14
14
ALTER TABLE t1 DROP PARTITION p21, p0;
SELECT LOWER(path) FROM information_schema.innodb_tablespaces AS `t`, information_schema.innodb_datafiles AS `df`
WHERE t.space = df.space
AND t.space_type = 'Single'
AND df.path NOT LIKE './mysql/%'
AND df.path NOT LIKE '%undo%'
AND df.path NOT LIKE '._mysql_%'
ORDER BY t.name;
LOWER(path)
./mtr/asserted_test_suppressions.ibd
./mtr/global_suppressions.ibd
./mtr/test_ignored_global_suppressions.ibd
./mtr/test_suppressions.ibd
./sys/sys_config.ibd
./test/t1#p#p1#sp#sp10.ibd
./test/t1#p#p1#sp#sp11.ibd
./test/t1#p#p22#sp#sp220.ibd
./test/t1#p#p22#sp#sp221.ibd
./test/tr.ibd
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=15 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p22 VALUES LESS THAN (2010)
 (SUBPARTITION sp220 ENGINE = InnoDB,
  SUBPARTITION sp221 ENGINE = InnoDB)) */
INSERT INTO t1 VALUES
(0, 'lava lamp', '2005-12-30'),
(0, 'lava lamp', '2005-12-31');
SELECT MAX(id) AS `Expect 16` FROM t1;
Expect 16
16
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(50) DEFAULT NULL,
  `purchased` date DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=17 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (year(`purchased`))
SUBPARTITION BY HASH (year(`purchased`))
(PARTITION p1 VALUES LESS THAN (2000)
 (SUBPARTITION sp10 ENGINE = InnoDB,
  SUBPARTITION sp11 ENGINE = InnoDB),
 PARTITION p22 VALUES LESS THAN (2010)
 (SUBPARTITION sp220 ENGINE = InnoDB,
  SUBPARTITION sp221 ENGINE = InnoDB)) */
DROP TABLE t1;
DROP TABLE tr;
