##########################################################################
# Test script to test ROLLBACK and RECOVERY for table having INSTANT
# ADD/DROP columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--echo # ------------------------------------------------------------
--echo # Create a table with 3 columns. [id, c1, c2, c3]
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1(id INT PRIMARY KEY, c1 VARCHAR(4000), c2 VARCHAR(4000), c3 VARCHAR(1000)) ROW_FORMAT=$row_format;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

INSERT INTO t1 VALUES(1, repeat('a', 4000), repeat('b', 4000), repeat('c', 1));
SELECT id, length(c1), length(c2), length(c3) FROM t1;
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), c3 FROM t1;

--echo # INSTANT ADD a column
ALTER TABLE t1 ADD COLUMN c4 VARCHAR(500) NOT NULL DEFAULT 'dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd', ALGORITHM=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

INSERT INTO t1 VALUES(2, repeat('e', 4000), repeat('f', 4000), repeat('g', 1), repeat('h', 100));

SELECT id, length(c1), length(c2), length(c3), length(c4) FROM t1;
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), c3, LEFT(c4, 10) FROM t1;

START TRANSACTION;
UPDATE t1 SET c1 = repeat('x', 200) WHERE id = 1;
SELECT id, length(c1), length(c2), length(c3), length(c4) FROM t1;
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), c3, LEFT(c4, 10) FROM t1;
ROLLBACK;

SELECT id, length(c1), length(c2), length(c3), length(c4) FROM t1;
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), c3, LEFT(c4, 10) FROM t1;

--echo # INSTANT DROP a column
ALTER TABLE t1 DROP COLUMN c3, ALGORITHM=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT id, length(c1), length(c2), length(c4) FROM t1;
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), LEFT(c4, 10) FROM t1;

START TRANSACTION;
UPDATE t1 SET c1 = repeat('x', 200) WHERE id = 1;
SELECT id, length(c1), length(c2), length(c4) FROM t1;
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), LEFT(c4, 10) FROM t1;
ROLLBACK;

SELECT id, length(c1), length(c2), length(c4) FROM t1;
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), LEFT(c4, 10) FROM t1;

--echo ###########
--echo # CLEANUP #
--echo ###########
DROP TABLE t1;
