################################################################################
# WL10905 - Support for INVISIBLE columns.                                     #
################################################################################

--echo #########################################################################
--echo # Test cases common to all the storage engines.                         #
--echo #########################################################################
--source include/invisible_columns.inc


--echo #########################################################################
--echo # Test cases specific to InnoDB storage engine.                         #
--echo #########################################################################

SET SESSION DEFAULT_STORAGE_ENGINE= InnoDB;

--echo #------------------------------------------------------------------------
--echo # Test case to verify INVISIBLE columns with CREATE TEMPORARY TABLE LIKE
--echo #------------------------------------------------------------------------
CREATE TABLE t1(f1 INT INVISIBLE, f2 INT);
CREATE TEMPORARY TABLE t2 LIKE t1;
SELECT * FROM t2;
SHOW CREATE TABLE t2;
SHOW COLUMNS FROM t2;
DROP TABLE t1, t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify foreign key constraint on a primary key column
--echo # referencing an invisible column.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT PRIMARY KEY INVISIBLE);
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT,
                 CONSTRAINT FOREIGN KEY (f1) REFERENCES t1(f2));
SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;

INSERT INTO t1(f1, f2) VALUES(1, 1);
INSERT INTO t2 VALUES (1, 2);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t2 VALUES (2, 3);
DROP TABLE t1, t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify Foreign Key Constraint on non-index column
--echo # referencing an invisible column.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT PRIMARY KEY INVISIBLE);
CREATE TABLE t2 (f1 INT, f2 INT, CONSTRAINT FOREIGN KEY (f1) REFERENCES t1(f2));
SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;

INSERT INTO t1(f1, f2) VALUES(1, 1);
INSERT INTO t2 VALUES (1, 2);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t2 VALUES (2, 3);
DROP TABLE t1, t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify Foreign Key Constraint on invisible column
--echo # referencing a visible column.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT PRIMARY KEY INVISIBLE, f2 INT,
                 CONSTRAINT FOREIGN KEY (f1) REFERENCES t1(f2));
SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;

INSERT INTO t1 VALUES(1, 1);
INSERT INTO t2(f1, f2) VALUES (1, 2);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t2(f1, f2) VALUES (2, 3);
DROP TABLE t1, t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify add Foreign Key Constraint referencing an invisible
--echo # column to an existing table.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT PRIMARY KEY INVISIBLE);
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT);
SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;
INSERT INTO t1(f1, f2) VALUES (1, 1);

ALTER TABLE t2 ADD CONSTRAINT FOREIGN KEY (f1) REFERENCES t1(f2);
SHOW CREATE TABLE t2;
INSERT INTO t2 VALUES (1, 3);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t2 VALUES (2, 3);
DROP TABLE t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify drop Foreign Key Constraint referencing an
--echo # invisible column.
--echo #------------------------------------------------------------------------
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT,
                 CONSTRAINT FOREIGN KEY (f1) REFERENCES t1(f2));
SHOW CREATE TABLE t2;
ALTER TABLE t2 DROP CONSTRAINT t2_ibfk_1;
SHOW CREATE TABLE t2;
DROP TABLE t1, t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify column visibility alter using INPLACE algorithm.
--echo #------------------------------------------------------------------------
CREATE TABLE t1 (f1 INT, f2 INT);
ALTER TABLE t1 ALTER COLUMN f1 SET INVISIBLE, ALGORITHM = INPLACE;
SHOW CREATE TABLE t1;
ALTER TABLE t1 CHANGE f1 f1 INT VISIBLE, ALGORITHM = INPLACE;
SHOW CREATE TABLE t1;
ALTER TABLE t1 MODIFY f1 INT INVISIBLE, ALGORITHM = INPLACE;
SHOW CREATE TABLE t1;
ALTER TABLE t1 ALTER COLUMN f1 SET INVISIBLE, ALGORITHM = INPLACE;
SHOW CREATE TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify column visibility alter using INSTANT algorithm.
--echo #------------------------------------------------------------------------
ALTER TABLE t1 ALTER COLUMN f1 SET INVISIBLE, ALGORITHM = INSTANT;
SHOW CREATE TABLE t1;
ALTER TABLE t1 CHANGE f1 f1 INT VISIBLE, ALGORITHM = INSTANT;
SHOW CREATE TABLE t1;
ALTER TABLE t1 MODIFY f1 INT INVISIBLE, ALGORITHM = INSTANT;
SHOW CREATE TABLE t1;
ALTER TABLE t1 ALTER COLUMN f1 SET INVISIBLE, ALGORITHM = INSTANT;
SHOW CREATE TABLE t1;
DROP TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify Partition table with INVISIBLE column.
--echo #------------------------------------------------------------------------
--echo # Range partition
CREATE TABLE t1(a INT, b DATE NOT NULL INVISIBLE)
        PARTITION BY RANGE( YEAR(b) ) (
        PARTITION p0 VALUES LESS THAN (1960),
        PARTITION p1 VALUES LESS THAN (1970),
        PARTITION p2 VALUES LESS THAN (1980),
        PARTITION p3 VALUES LESS THAN (1990));
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;
INSERT INTO t1(a, b) VALUES(1, '1960-01-01');
SELECT a, b FROM t1;
DROP TABLE t1;

--echo # List partition
CREATE TABLE t1(id INT NOT NULL INVISIBLE, name VARCHAR(10))
        PARTITION BY LIST(id) (
        PARTITION p0 VALUES IN (10,19),
        PARTITION p1 VALUES IN (20,29),
        PARTITION p2 VALUES IN (30,39),
        PARTITION p3 VALUES IN (40,49));
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;
INSERT INTO t1(id, name) VALUES(30,'aaa');
SELECT id, name FROM t1;
DROP TABLE t1;

--echo # Hash partition
CREATE TABLE t1(id INT NOT NULL INVISIBLE, name VARCHAR(40))
        PARTITION BY HASH(id)
        PARTITIONS 4;
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;
INSERT INTO t1(id, name) VALUES(60,'aaa');
SELECT id, name FROM t1;
DROP TABLE t1;

--echo # Key partition
CREATE TABLE t1(id INT PRIMARY KEY NOT NULL INVISIBLE, name VARCHAR(40))
        PARTITION BY KEY()
        PARTITIONS 4;
SHOW CREATE TABLE t1;
SHOW COLUMNS FROM t1;
INSERT INTO t1(id, name) VALUES(60,'aaa');
SELECT id, name FROM t1;
DROP TABLE t1;

SET SESSION DEFAULT_STORAGE_ENGINE= DEFAULT;


--echo #------------------------------------------------------------------------
--echo # Bug#33781534 - I_S.KEY_COLUMN_USAGE does not lists invisible columns
--echo #                which has key constraints.
--echo #------------------------------------------------------------------------

CREATE TABLE t1 (f1 INT PRIMARY KEY INVISIBLE, f2 INT UNIQUE INVISIBLE,
                 f3 INT, FOREIGN KEY (f2) REFERENCES t1(f1));
SHOW CREATE TABLE t1;
--echo # Columns with invisible attribute are listed by the INFORMATION_SCHEMA.COLUMNS.
--sorted_result
SELECT TABLE_NAME, COLUMN_NAME, EXTRA FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_NAME='t1';
--echo # Without fix, following query returns empty resultset. With fix, key columns
--echo # with invisible attribute should be listed.
--sorted_result
SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, REFERENCED_COLUMN_NAME
       FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_NAME='t1';
DROP TABLE t1;
