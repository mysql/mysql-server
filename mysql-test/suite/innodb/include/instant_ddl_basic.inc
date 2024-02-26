##########################################################################
# Test script to test ALTER TABLE ADD/DROP COLUMN ... ALGORITHM=INSTANT
# at different places for table with
#  - No index defined
#  - Cluster index defined
#  - Secondary index defined
#
# This test is to be run in debug mode only and with debug flag
# $row_format  : indicates the row_format on which test is to be run.
# $clust_index : indicates Primary key clause (if any)
# $sec_index   : indicates secondary key clause (if any)
##########################################################################

--echo # ------------------------------------------------------------
--echo # Create a table with 3 columns. [c1, c2, c3]
--echo # ------------------------------------------------------------
--eval CREATE TABLE t1 (c1 char(20), c2 char(20), c3 char(20) $clust_index $sec_index) ROW_FORMAT=$row_format

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

INSERT INTO t1 values ("r1c1", "r1c2", "r1c3");
SELECT * FROM t1 ORDER BY c1;

--echo
--echo # -----------------------ADD COLUMN TEST----------------------
--echo

--echo # ------------------------------------------------------------
--echo # Add a new column at the end. [c1, c2, c3, +c4]
--echo # ------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c4 char(20) DEFAULT "c4_def", ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

INSERT INTO t1 values ("r2c1", "r2c2", "r2c3", "r2c4");
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Add a new column in between. [c1, +c5, c2, c3, c4]
--echo # ------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c5 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

INSERT INTO t1 values ("r3c1", "r3c5", "r3c2", "r3c3", "r3c4");
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Add a new column at first. [+c6, c1, c5, c2, c3, c4]
--echo # ------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c6 char(20) DEFAULT NULL FIRST, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

INSERT INTO t1 values ("r4c6", "r4c1", "r4c5", "r4c2", "r4c3", "r4c4");
SELECT * FROM t1 ORDER BY c1;


--echo # ------------------------------------------------------------
--echo # Rebuild table. [c6, c1, c5, c2, c3, c4]
--echo # ------------------------------------------------------------
ALTER TABLE t1 force;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1 ORDER BY c1;

--echo
--echo # -----------------------DROP COLUMN TEST---------------------
--echo

--echo # ------------------------------------------------------------
--echo # Drop a column from the end. [c6, c1, c5, c2, c3, -c4]
--echo # ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c4, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 values ("r5c6", "r5c1", "r5c5", "r5c2", "r5c3");
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Drop a column somewhere in between. [c6, c1, -c5, c2, c3] [~c4]
--echo # ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c5, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 values ("r6c6", "r6c1", "r6c2", "r6c3");
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Drop the first column. [-c6, c1, c2, c3] [~c4, ~c5]
--echo # ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c6, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 values ("r7c1", "r7c2", "r7c3");
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # ADD and DROP in same statement. [c1, c2, -c3, +c7] [~c4, ~c5, ~c6]
--echo # ------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c7 char(10) default "c7_def", DROP COLUMN c3, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 values ("r8c1", "r8c2", "r8c7");
SELECT * FROM t1 ORDER BY c1;

--echo # --------------------------------------------------------------------------
--echo # ADD and DROP same column in same statement. [c1, c2, -c7, +c7] [~c4, ~c5, ~c6, ~c3]
--echo # --------------------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c7 char(10) default "c7_def2", DROP COLUMN c7, ALGORITHM=INSTANT;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 values ("r9c1", "r9c2", "r9c7");
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Rebuild table. [c1, c2, c7]
--echo # ------------------------------------------------------------
ALTER TABLE t1 force;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 values ("r10c1", "r10c2", "r10c7");
SELECT * FROM t1 ORDER BY c1;

--echo ###########
--echo # CLEANUP #
--echo ###########
DROP TABLE t1;

#
# Bug #33937504: INSTANT ADD COLUMNS are visible in older read view created before INSTANT DDL
#

--connect(con2,localhost,root,,)
--connect(con3,localhost,root,,)


--let $drop=0

while ($drop <= 1) {

  --let $use_instant=0
  while ($use_instant <= 1) {

    connection con2;
    --let $table_name=t2
    --eval CREATE TABLE $table_name (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format
    INSERT INTO t2 VALUES ("r1c1", "r1c2");
    SELECT * FROM t2;
  
    --let $table_name=t1
    --eval CREATE TABLE $table_name (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format
    INSERT INTO t1 VALUES ("r1c1", "r1c2");
    SELECT * FROM t1;
  
    BEGIN;
    SELECT * FROM t2;
  
    connection con3;
    if ($use_instant == 0) {
      if ($drop == 0) {
        ALTER TABLE t1 ADD COLUMN c3 char(10) DEFAULT "c3_def",
        ALGORITHM=COPY;
      }
      if ($drop == 1) {
        ALTER TABLE t1 DROP COLUMN c2,
        ALGORITHM=COPY;
      }
    }

    if ($use_instant == 1) {
      if ($drop == 0) {
        ALTER TABLE t1 ADD COLUMN c3 char(10) DEFAULT "c3_def",
        ALGORITHM=INSTANT;
      }
      if ($drop == 1) {
        ALTER TABLE t1 DROP COLUMN c2,
        ALGORITHM=INSTANT;
      }
    }
  
    UPDATE t1 SET c1 = "r1c11";
    SELECT * FROM t1;
  
    connection con2;
    --error ER_TABLE_DEF_CHANGED
    SELECT * FROM t1;
  
    DROP TABLE t1;
    DROP TABLE t2;
    
    --inc $use_instant
  }
  --inc $drop
}

connection default;

disconnect con2;
disconnect con3;
