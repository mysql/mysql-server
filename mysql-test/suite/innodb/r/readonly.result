#
# Bug#19904800 - MISSING DBUG_RETURN IN HA_INNOBASE::UPDATE_ROW
#
CREATE TABLE t1(a int PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t2(b int PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES(1);
INSERT INTO t2 VALUES(2);
SET GLOBAL innodb_fast_shutdown = 0;
# restart: --innodb-read-only
UPDATE t1,t2 SET t1.a = 2, t2.b = 2 WHERE t1.a = 1;
ERROR HY000: Table 't1' is read only
# restart
DROP TABLE t1;
DROP TABLE t2;
#
# Bug#20578834 - INNODB READ ONLY MODE AND NON EXISTENT TMP DIR CRASHES SERVER
#
CREATE TABLE t1(a int PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES(1);
# Patch for bug#31377118 rejects startup if tmpdir does not exist.
Occurrences of 'No such file or directory' in the input file: 1
# restart
SELECT * FROM t1;
a
1
DROP TABLE t1;
#
# Bug#21966795 "FK DEFINITIONS MISSING FROM SHOW CREATE TABLE IN
#               'INNODB_READ_ONLY' MODE"
#
CREATE TABLE parent(pk INT PRIMARY KEY);
CREATE TABLE child(fk INT, CONSTRAINT c FOREIGN KEY (fk) REFERENCES parent (pk));
SHOW CREATE TABLE child;
Table	Create Table
child	CREATE TABLE `child` (
  `fk` int DEFAULT NULL,
  KEY `c` (`fk`),
  CONSTRAINT `c` FOREIGN KEY (`fk`) REFERENCES `parent` (`pk`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL innodb_fast_shutdown = 0;
# restart: --innodb-read-only
# SHOW CREATE TABLE should still show foreign keys.
SHOW CREATE TABLE child;
Table	Create Table
child	CREATE TABLE `child` (
  `fk` int DEFAULT NULL,
  KEY `c` (`fk`),
  CONSTRAINT `c` FOREIGN KEY (`fk`) REFERENCES `parent` (`pk`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Clean-up.
# restart
DROP TABLES child, parent;
