##########################################################################
# Test script to test EXPORT/IMPORT for table having INSTANT ADD/DROP
# columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--write_file $MYSQLTEST_VARDIR/tmp/export.inc EOF
  FLUSH TABLE t1 FOR EXPORT;
  --echo # Copy cfg/ibd files into temp location
  --copy_file $MYSQLD_DATADIR/test/t1.cfg $MYSQLD_DATADIR/t1.cfg_back
  --copy_file $MYSQLD_DATADIR/test/t1.ibd $MYSQLD_DATADIR/t1.ibd_back
  UNLOCK TABLES;
EOF

--write_file $MYSQLTEST_VARDIR/tmp/import.inc EOF
  ALTER TABLE t1 DISCARD TABLESPACE;
  --echo # Copy cfg/ibd files from temp location
  --copy_file $MYSQLD_DATADIR/t1.cfg_back $MYSQLD_DATADIR/test/t1.cfg
  --copy_file $MYSQLD_DATADIR/t1.ibd_back $MYSQLD_DATADIR/test/t1.ibd
  --error $err_no
  ALTER TABLE t1 IMPORT TABLESPACE;
EOF

--let $MYSQLD_DATADIR = `SELECT @@datadir`

--echo # Create a table with 2 columns. [c1, c2]
--eval CREATE TABLE t1 (c1 char(20) KEY, c2 enum('a', 'b', 'c')) ROW_FORMAT=$row_format;
INSERT INTO t1 values ("row1_c1", "a");
SELECT * FROM t1 ORDER BY c1;

--echo # Add a new column at the end. [c1, c2, +c3]
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "default_c3", ALGORITHM=INSTANT;
INSERT INTO t1 values ("row2_c1", "b", "row2_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # Add a new column in between. [c1, +c4, c2, c3]
ALTER TABLE t1 ADD COLUMN c4 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;
INSERT INTO t1 values ("row3_c1", "row3_c4", "c", "row3_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Scenario 1.1 : Same table with tablespace discarded
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

--echo # IMPORT
--let $err_no=0
--source $MYSQLTEST_VARDIR/tmp/import.inc

SELECT * FROM t1 ORDER BY c1;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back

--echo # ------------------------------------------------------------
--echo # Scenario 1.2 : New table created with like
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

CREATE TABLE t2 like t1;
DROP TABLE t1;
ALTER TABLE t2 RENAME to t1;
SHOW CREATE TABLE t1;

--echo # IMPORT
--let $err_no=0
--source $MYSQLTEST_VARDIR/tmp/import.inc

SELECT * FROM t1;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back


--echo # ------------------------------------------------------------
--echo # Drop a column somewhere in between. [c1, c4, -c2, c3]
--echo # ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1 ORDER BY c1;

INSERT INTO t1 values ("row4_c1", "row4_c4", "row4_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Scenario 2.1 : Same table with tablespace discarded
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

--echo # IMPORT
--let $err_no=0
--source $MYSQLTEST_VARDIR/tmp/import.inc

SELECT * FROM t1 ORDER BY c1;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back

--echo # ------------------------------------------------------------
--echo # Scenario 2.2 : New table created with like
--echo # ------------------------------------------------------------
--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc

CREATE TABLE t2 like t1;
DROP TABLE t1;
ALTER TABLE t2 RENAME to t1;
SHOW CREATE TABLE t1;

--echo # IMPORT
--let $err_no=0
--source $MYSQLTEST_VARDIR/tmp/import.inc

SELECT * FROM t1;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back
DROP TABLE t1;

--echo # ------------------------------------------------------------
--echo # Scenarios 3 : Error scenarios
--echo # ------------------------------------------------------------

--echo # ------------------------------------------------------------
--echo # Scenario 3.1 : Target table has INSTANT, source doesn't
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20)) ROW_FORMAT=$row_format;
INSERT INTO t1 values ("row1_c1", "row1_c2");
SELECT * FROM t1 ORDER BY c1;

--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc
DROP TABLE t1;

--eval CREATE TABLE t1 (c1 char(20) KEY) ROW_FORMAT=$row_format;
ALTER TABLE t1 ADD COLUMN c2 char(20) DEFAULT "def_c2", ALGORITHM=INSTANT;

--echo # IMPORT
--let $err_no=ER_TABLE_SCHEMA_MISMATCH
--source $MYSQLTEST_VARDIR/tmp/import.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back
--remove_file $MYSQLD_DATADIR/test/t1.cfg
--remove_file $MYSQLD_DATADIR/test/t1.ibd
DROP TABLE t1;

--echo # ------------------------------------------------------------
--echo # Scenario 3.2 : Both has INSTANT, but different instant version
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20)) ROW_FORMAT=$row_format;
INSERT INTO t1 values ("row1_c1", "row1_c2");
SELECT * FROM t1 ORDER BY c1;
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "default_c3", ALGORITHM=INSTANT;
INSERT INTO t1 values ("row2_c1", "row2_c2", "row2_c3");
SELECT * FROM t1 ORDER BY c1;
ALTER TABLE t1 ADD COLUMN c4 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;
INSERT INTO t1 values ("row3_c1", "row3_c4", "row3_c2", "row3_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc
DROP TABLE t1;

--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20), c3 char(20) DEFAULT "default_c3") ROW_FORMAT=$row_format;
ALTER TABLE t1 ADD COLUMN c4 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;

--echo # IMPORT
--let $err_no=ER_TABLE_SCHEMA_MISMATCH
--source $MYSQLTEST_VARDIR/tmp/import.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back
--remove_file $MYSQLD_DATADIR/test/t1.cfg
--remove_file $MYSQLD_DATADIR/test/t1.ibd
DROP TABLE t1;

--echo # ------------------------------------------------------------
--echo # Scenario 3.3 : Both has INSTANT, but different col count
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20)) ROW_FORMAT=$row_format;
INSERT INTO t1 values ("row1_c1", "row1_c2");
SELECT * FROM t1 ORDER BY c1;
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "default_c3", ALGORITHM=INSTANT;
INSERT INTO t1 values ("row2_c1", "row2_c2", "row2_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc
DROP TABLE t1;

--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20), c3 char(20) DEFAULT "default_c3") ROW_FORMAT=$row_format;
ALTER TABLE t1 ADD COLUMN c4 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;

--echo # IMPORT
--let $err_no=ER_TABLE_SCHEMA_MISMATCH
--source $MYSQLTEST_VARDIR/tmp/import.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back
--remove_file $MYSQLD_DATADIR/test/t1.cfg
--remove_file $MYSQLD_DATADIR/test/t1.ibd
DROP TABLE t1;

--echo # ------------------------------------------------------------
--echo # Scenario 3.4 : Both has INSTANT, but different col count
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20)) ROW_FORMAT=$row_format;
INSERT INTO t1 values ("row1_c1", "row1_c2");
SELECT * FROM t1 ORDER BY c1;
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "default_c3", ADD COLUMN c4 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;
INSERT INTO t1 values ("row2_c1", "row2_c4", "row2_c2", "row2_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc
DROP TABLE t1;

--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20), c3 char(20) DEFAULT "default_c3") ROW_FORMAT=$row_format;
ALTER TABLE t1 ADD COLUMN c4 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;

--echo # IMPORT
--let $err_no=ER_TABLE_SCHEMA_MISMATCH
--source $MYSQLTEST_VARDIR/tmp/import.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back
--remove_file $MYSQLD_DATADIR/test/t1.cfg
--remove_file $MYSQLD_DATADIR/test/t1.ibd
DROP TABLE t1;

--echo # ------------------------------------------------------------
--echo # Scenario 3.5 : Both has INSTANT, but different default value
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20)) ROW_FORMAT=$row_format;
INSERT INTO t1 values ("row1_c1", "row1_c2");
SELECT * FROM t1 ORDER BY c1;
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "default_c3", ALGORITHM=INSTANT;
INSERT INTO t1 values ("row2_c1", "row2_c2", "row2_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # EXPORT
--source $MYSQLTEST_VARDIR/tmp/export.inc
DROP TABLE t1;

--eval CREATE TABLE t1 (c1 char(20) KEY, c2 char(20)) ROW_FORMAT=$row_format;
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "newdefault_c3", ALGORITHM=INSTANT;

--echo # IMPORT
--let $err_no=ER_TABLE_SCHEMA_MISMATCH
--source $MYSQLTEST_VARDIR/tmp/import.inc

--remove_file $MYSQLD_DATADIR/t1.cfg_back
--remove_file $MYSQLD_DATADIR/t1.ibd_back
--remove_file $MYSQLD_DATADIR/test/t1.cfg
--remove_file $MYSQLD_DATADIR/test/t1.ibd
DROP TABLE t1;

--echo ###########
--echo # CLEANUP #
--echo ###########
--remove_file $MYSQLTEST_VARDIR/tmp/import.inc
--remove_file $MYSQLTEST_VARDIR/tmp/export.inc
