#
# Bug #27453180 FOREIGN KEYS CONSTRAINTS IGNORED AFTER RENAME TABLE
#
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE child (fk INT, FOREIGN KEY (fk) REFERENCES parent(pk));
show create table child;
Table	Create Table
child	CREATE TABLE `child` (
  `fk` int DEFAULT NULL,
  KEY `fk` (`fk`),
  CONSTRAINT `child_ibfk_1` FOREIGN KEY (`fk`) REFERENCES `parent` (`pk`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN;
ID	FOR_NAME	REF_NAME	N_COLS	TYPE
test/child_ibfk_1	test/child	test/parent	1	48
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS;
ID	FOR_COL_NAME	REF_COL_NAME	POS
test/child_ibfk_1	fk	pk	1
INSERT INTO child VALUES (1);
CREATE TABLE parent0 (pk INT PRIMARY KEY);
INSERT INTO parent0 VALUES (1);
RENAME TABLE parent0 TO parent;
SET FOREIGN_KEY_CHECKS=1;
DELETE FROM parent;
ERROR 23000: Cannot delete or update a parent row: a foreign key constraint fails (`test`.`child`, CONSTRAINT `child_ibfk_1` FOREIGN KEY (`fk`) REFERENCES `parent` (`pk`))
DROP TABLE child;
DROP TABLE parent;
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE child (fk INT, FOREIGN KEY (fk) REFERENCES parent(pk));
show create table child;
Table	Create Table
child	CREATE TABLE `child` (
  `fk` int DEFAULT NULL,
  KEY `fk` (`fk`),
  CONSTRAINT `child_ibfk_1` FOREIGN KEY (`fk`) REFERENCES `parent` (`pk`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN;
ID	FOR_NAME	REF_NAME	N_COLS	TYPE
test/child_ibfk_1	test/child	test/parent	1	48
SELECT * FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS;
ID	FOR_COL_NAME	REF_COL_NAME	POS
test/child_ibfk_1	fk	pk	1
INSERT INTO child VALUES (1);
CREATE TABLE parent0 (pk INT PRIMARY KEY);
INSERT INTO parent0 VALUES (1);
ALTER TABLE parent0 RENAME parent;
SET FOREIGN_KEY_CHECKS=1;
DELETE FROM parent;
ERROR 23000: Cannot delete or update a parent row: a foreign key constraint fails (`test`.`child`, CONSTRAINT `child_ibfk_1` FOREIGN KEY (`fk`) REFERENCES `parent` (`pk`))
DROP TABLE child;
DROP TABLE parent;
