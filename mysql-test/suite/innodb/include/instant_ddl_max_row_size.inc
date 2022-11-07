##########################################################################
# Test script to test ALTER TABLE ADD/DROP COLUMN ... ALGORITHM=INSTANT
# when INSTANT ADD causes max row size to go beyond permissible limit
#
# This test is to be run in debug mode only and with debug flag
##########################################################################

--disable_query_log
call mtr.add_suppression("\\[Error\\] .*MY-\\d+.* Cannot add field.*");
call mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* Cannot add field.*");
--enable_query_log
SET @saved_innodb_strict_mode=@@SESSION.innodb_strict_mode;

--echo # ------------------------------------------------------------
--echo # Create a table with 3 columns. [id, c2, c3]
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1(id INT PRIMARY KEY, c1 VARCHAR(4000), c2 VARCHAR(4000)) ROW_FORMAT=$row_format;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SET SESSION innodb_strict_mode=OFF;

--echo # ------------------------------------------------------------
--echo # Add a new column. It causes max_row_size > permissible size
--echo # ------------------------------------------------------------
# Error with ALGORITHM=INSTANT
--error ER_INNODB_INSTANT_ADD_DROP_NOT_SUPPORTED_MAX_SIZE
ALTER TABLE t1 ADD COLUMN c3 VARCHAR(500) NOT NULL DEFAULT 'dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd', ALGORITHM=INSTANT;

# Pass with DEFAULT ALGORITHM
ALTER TABLE t1 ADD COLUMN c3 VARCHAR(500) NOT NULL DEFAULT 'dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd';

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

# This INSERT results in error as the row size goes beyond permissible 
--error ER_TOO_BIG_ROWSIZE
INSERT INTO t1 (id, c1, c2) VALUES(1, repeat('a', 4000), repeat('b', 4000));

# This INSERT succeeds
INSERT INTO t1 (id, c1, c2) VALUES(1, repeat('a', 4), repeat('b', 4));
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), LEFT(c3, 10) FROM t1 ORDER BY id;

# Now this table is already in a state where a possible row could go beyond
# permissible size. So no new column could be added with ALGORITHM=INSTANT
--error ER_INNODB_INSTANT_ADD_DROP_NOT_SUPPORTED_MAX_SIZE
ALTER TABLE t1 ADD COLUMN c4 INT, ALGORITHM=INSTANT;

# Now this table is already in a state where a possible row could go beyond
# permissible size. So no column could be dropped with ALGORITHM=INSTANT
--error ER_INNODB_INSTANT_ADD_DROP_NOT_SUPPORTED_MAX_SIZE
ALTER TABLE t1 DROP COLUMN c3, ALGORITHM=INSTANT;

--echo ###########
--echo # CLEANUP #
--echo ###########
DROP TABLE t1;
SET SESSION innodb_strict_mode=@saved_innodb_strict_mode;

--echo # ------------------------------------------------------------
--echo # Create a partition table with 3 columns. [c1, c2, c3, c4]
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1 (c1 INT, c2 INT, c3 INT, c4 TEXT) ROW_FORMAT=$row_format PARTITION BY RANGE(c1 * 2) ( PARTITION p0 VALUES LESS THAN (128), PARTITION p1 VALUES LESS THAN (256) , PARTITION p2 VALUES LESS THAN (384) , PARTITION p3 VALUES LESS THAN MAXVALUE);

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

SET SESSION innodb_strict_mode=OFF;

--echo # ------------------------------------------------------------
--echo # Add new columns. It causes max_row_size > permissible size
--echo # ------------------------------------------------------------
--error ER_INNODB_INSTANT_ADD_DROP_NOT_SUPPORTED_MAX_SIZE
ALTER TABLE t1 ADD COLUMN c5 VARCHAR(500) NOT NULL DEFAULT 'dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd', ADD COLUMN c6 TEXT, ALGORITHM=INSTANT;

# Pass with DEFAULT ALGORITHM
ALTER TABLE t1 ADD COLUMN c5 VARCHAR(500) NOT NULL DEFAULT 'dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd', ADD COLUMN c6 TEXT;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

# This INSERT results in error as the row size goes beyond permissible
--error ER_TOO_BIG_ROWSIZE
INSERT INTO t1 (c1, c2, c3, c4, c6) VALUES(1, 2, 3, repeat('a', 1000), repeat('c', 1000));

# This INSERT succeeds
INSERT INTO t1 (c1, c2, c3, c4, c6) VALUES(1, 2, 3, 'abcdefg', 'hijklmnop');
SELECT c1, c2, c3, c4, LEFT(c5 , 10), c6 from t1;

# Now this table is already in a state where a possible row could go beyond
# permissible size. So no new column could be added with ALGORITHM=INSTANT
--error ER_INNODB_INSTANT_ADD_DROP_NOT_SUPPORTED_MAX_SIZE
ALTER TABLE t1 ADD COLUMN c7 INT, ALGORITHM=INSTANT;

# Now this table is already in a state where a possible row could go beyond
# permissible size. So no column could be dropped with ALGORITHM=INSTANT
--error ER_INNODB_INSTANT_ADD_DROP_NOT_SUPPORTED_MAX_SIZE
ALTER TABLE t1 DROP COLUMN c6, ALGORITHM=INSTANT;

--echo ###########
--echo # CLEANUP #
--echo ###########
DROP TABLE t1;
SET SESSION innodb_strict_mode=@saved_innodb_strict_mode;
