##########################################################################
# Test script to test EXPORT/IMPORT for partitioned table having INSTANT
# ADD/DROP columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--write_file $MYSQLTEST_VARDIR/tmp/export.inc EXECUTE_STEP
  FLUSH TABLE t1 FOR EXPORT;
  perl;
  require 'include/innodb-util.inc';
  ib_backup_tablespaces("test", "t1#p#p0", "t1#p#p1", "t1#p#p2", "t1#p#p3");
  EOF
  UNLOCK TABLES;
EXECUTE_STEP

--write_file $MYSQLTEST_VARDIR/tmp/import.inc EXECUTE_STEP
  ALTER TABLE t1 discard tablespace;
  perl;
  require 'include/innodb-util.inc';
  ib_restore_tablespaces("test", "t1#p#p0", "t1#p#p1", "t1#p#p2", "t1#p#p3");
  EOF
  ALTER TABLE t1 IMPORT TABLESPACE;
EXECUTE_STEP

--write_file $MYSQLTEST_VARDIR/tmp/cleanup.inc EXECUTE_STEP
  --remove_files_wildcard $MYSQLTEST_VARDIR/tmp/ t1*
EXECUTE_STEP

--echo # Create a table with 4 columns. [c1, c2, c3, c4]
--eval CREATE TABLE t1 (c1 INT, c2 INT, c3 INT, c4 TEXT) ROW_FORMAT=$row_format PARTITION BY RANGE(c1 * 2) (PARTITION p0 VALUES LESS THAN (128), PARTITION p1 VALUES LESS THAN (256), PARTITION p2 VALUES LESS THAN (384), PARTITION p3 VALUES LESS THAN MAXVALUE)

INSERT INTO t1 VALUES(1, 2, 3, 'abcdefg'), (100, 200, 300, 'qwerty'), (200, 300, 400, 'asdfg');
SELECT * from t1;

--echo # Add a new column at the end. [c1, c2, c3, c4, +c5]
ALTER TABLE t1 ADD COLUMN c5 CHAR(20) default "c5_def", ALGORITHM=INSTANT;
INSERT INTO t1 VALUES(300, 400, 500, "xxxxxx", "r4c5") ;
SELECT * FROM t1 ORDER BY c1;

--echo # Add a new column in between. [c1, +c6, c2, c3, c4, c5]
ALTER TABLE t1 ADD COLUMN c6 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;
INSERT INTO t1 VALUES(400, "r5c6", 500, 600, "xxxxxx", "r5c5") ;
SELECT * FROM t1 ORDER BY c1;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

let MYSQLD_DATADIR = `SELECT @@datadir`;

--echo # ------------------------------------------------------------
--echo # Scenario 1.1 : Same table with tablespace discarded
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

--echo # IMPORT
--source $MYSQLTEST_VARDIR/tmp/import.inc

SELECT * FROM t1 ORDER BY c1;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

--source $MYSQLTEST_VARDIR/tmp/cleanup.inc

--echo # ------------------------------------------------------------
--echo # Scenario 1.2 : New table created with like
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

CREATE TABLE t2 like t1;
SHOW CREATE TABLE t2;
DROP TABLE t1;
ALTER TABLE t2 RENAME as t1;

--echo # IMPORT
--source $MYSQLTEST_VARDIR/tmp/import.inc

SELECT * FROM t1 ORDER BY c1;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

--source $MYSQLTEST_VARDIR/tmp/cleanup.inc

--echo # ------------------------------------------------------------
--echo # Drop a column somewhere in between. [c1, c6, -c2, c3, c4, c5]
--echo # ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 VALUES(500, "r6c6", 700, "xxxxxx", "r6c5") ;
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Scenario 2.1 : Same table with tablespace discarded
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

--echo # IMPORT
--source $MYSQLTEST_VARDIR/tmp/import.inc

SELECT * FROM t1 ORDER BY c1;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

--source $MYSQLTEST_VARDIR/tmp/cleanup.inc

--echo # ------------------------------------------------------------
--echo # Scenario 2.2 : New table created with like
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

CREATE TABLE t2 like t1;
SHOW CREATE TABLE t2;
DROP TABLE t1;
ALTER TABLE t2 RENAME as t1;

--echo # IMPORT
--source $MYSQLTEST_VARDIR/tmp/import.inc
SELECT * FROM t1 ORDER BY c1;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

--source $MYSQLTEST_VARDIR/tmp/cleanup.inc
DROP TABLE t1;

--echo # ------------------------------------------------------------
--echo # A Partitioned table having multiple INSTANT ADD/DROP columns
--echo # and REORGNIZED partitions.
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1 (id INT PRIMARY KEY, name VARCHAR(50), job VARCHAR(50), dept INT, phoneno INT) ROW_FORMAT=$row_format PARTITION BY RANGE( id ) (PARTITION P0 VALUES LESS THAN (10), PARTITION P1 VALUES LESS THAN (20), PARTITION P2 VALUES LESS THAN (30), PARTITION P3 VALUES LESS THAN (40), PARTITION P4 VALUES LESS THAN (50))

INSERT INTO t1 VALUES(10,'aaa','xyz1',1,111);
INSERT INTO t1 VALUES(20,'bbb','xyz2',2,222);
INSERT INTO t1 VALUES(30,'ccc','xyz3',3,333);
INSERT INTO t1 VALUES(40,'ddd','xyz4',4,444);

ALTER TABLE t1 ADD COLUMN sal INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN phoneno, ALGORITHM=INSTANT;
ALTER TABLE t1 ADD COLUMN phoneno INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN sal, ALGORITHM=INSTANT;

ALTER TABLE t1 ADD COLUMN sal INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN phoneno, ALGORITHM=INSTANT;
ALTER TABLE t1 ADD COLUMN phoneno INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN sal, ALGORITHM=INSTANT;

ALTER TABLE t1 ADD COLUMN sal INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN phoneno, ALGORITHM=INSTANT;
ALTER TABLE t1 ADD COLUMN phoneno INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN sal, ALGORITHM=INSTANT;

ALTER TABLE t1 ADD COLUMN sal INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN phoneno, ALGORITHM=INSTANT;
ALTER TABLE t1 ADD COLUMN phoneno INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN sal, ALGORITHM=INSTANT;

ALTER TABLE t1 ADD COLUMN sal INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN phoneno, ALGORITHM=INSTANT;
ALTER TABLE t1 ADD COLUMN phoneno INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN sal, ALGORITHM=INSTANT;

ALTER TABLE t1 REORGANIZE PARTITION P3, P4 INTO (PARTITION P3 VALUES LESS THAN (50));

ALTER TABLE t1 ADD COLUMN sal INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN phoneno, ALGORITHM=INSTANT;

SELECT * FROM t1 ORDER BY id;

--echo # ------------------------------------------------------------
--echo # Scenario 3.1 : Same table with tablespace discarded
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

--echo # IMPORT
--source $MYSQLTEST_VARDIR/tmp/import.inc

SELECT * FROM t1 ORDER BY id;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

--source $MYSQLTEST_VARDIR/tmp/cleanup.inc

--echo # ------------------------------------------------------------
--echo # Scenario 3.2 : New table created with like
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

CREATE TABLE t2 like t1;
SHOW CREATE TABLE t2;
DROP TABLE t1;
ALTER TABLE t2 RENAME as t1;

--echo # IMPORT
--source $MYSQLTEST_VARDIR/tmp/import.inc
SELECT * FROM t1 ORDER BY id;

--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

--source $MYSQLTEST_VARDIR/tmp/cleanup.inc

DROP TABLE t1;

--echo ###########
--echo # CLEANUP #
--echo ###########
--remove_file $MYSQLTEST_VARDIR/tmp/import.inc
--remove_file $MYSQLTEST_VARDIR/tmp/export.inc
--remove_file $MYSQLTEST_VARDIR/tmp/cleanup.inc
