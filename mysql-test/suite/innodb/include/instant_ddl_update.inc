##########################################################################
# Test script to test UPDATE on table having INSTANT ADD/DROP columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--eval CREATE TABLE t1 (c1 char(20), c2 char(20)) ROW_FORMAT=$row_format
INSERT INTO t1 values ("row1_c1", "row1_c2");

SHOW CREATE TABLE t1;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc

--echo # ------------------------------------------------------------
--echo # ADD AN INSTANT COLUMN At the end [c1, c2, +c3]
--echo # ------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c3 char(20) default "c3_def", ALGORITHM=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
SELECT * FROM t1 ORDER BY c1;

INSERT INTO t1 values ("row2_c1", "row2_c2", "row2_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # INPLACE UPDATE
UPDATE t1 SET c1="r1_c1" where c1="row1_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # NOT INPLACE UPDATE
UPDATE t1 SET c3="row1_c3" where c1="r1_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # ADD AN INSTANT COLUMN At the first [+c0, c1, c2, c3]
--echo # ------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c0 char(20) default "c0_def" FIRST, ALGORITHM=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
SELECT * FROM t1 ORDER BY c1;

INSERT INTO t1 values ("row3_c0", "row3_c1", "row3_c2", "row3_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # INPLACE UPDATE
UPDATE t1 SET c1="r2_c1" where c1="row2_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # NOT INPLACE UPDATE
UPDATE t1 SET c0="row2_c0" where c1="r2_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # ADD AN INSTANT COLUMN in between [c0, c1, +c4, c2, c3]
--echo # ------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c4 char(20) default "c4_def" AFTER c1, ALGORITHM=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
SELECT * FROM t1 ORDER BY c1;

INSERT INTO t1 values ("row4_c0", "row4_c1", "row4_c4", "row4_c2", "row4_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # INPLACE UPDATE
UPDATE t1 SET c1="r3_c1" where c1="row3_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # NOT INPLACE UPDATE
UPDATE t1 SET c4="row3_c4" where c1="r3_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # DROP A COLUMN in between [c0, c1, c4, -c2, c3]
--echo # ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
SELECT * FROM t1 ORDER BY c1;

INSERT INTO t1 values ("row5_c0", "row5_c1", "row5_c4", "row5_c3");
SELECT * FROM t1 ORDER BY c1;

--echo # INPLACE UPDATE
UPDATE t1 SET c1="r4_c1" where c1="row4_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # NOT INPLACE UPDATE
UPDATE t1 SET c4="row1_c4" where c1="r1_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # DROP A COLUMN at the end [c0, c1, c4, ~c2, -c3]
--echo # ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c3, ALGORITHM=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
SELECT * FROM t1 ORDER BY c1;

INSERT INTO t1 values ("row6_c0", "row6_c1", "row6_c4");
SELECT * FROM t1 ORDER BY c1;

--echo # INPLACE UPDATE
UPDATE t1 SET c1="r5_c1" where c1="row5_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # NOT INPLACE UPDATE
UPDATE t1 SET c0="row1_c0" where c1="r1_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # DROP A COLUMN at the beginning [-c0, c1, c4, ~c2, ~c3]
--echo # ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c0, ALGORITHM=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
SELECT * FROM t1 ORDER BY c1;

INSERT INTO t1 values ("row7_c1", "row7_c4");
SELECT * FROM t1 ORDER BY c1;

--echo # INPLACE UPDATE
UPDATE t1 SET c1="r6_c1" where c1="row6_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # NOT INPLACE UPDATE
UPDATE t1 SET c4="row2_c4" where c1="r2_c1";
SELECT * FROM t1 ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Rebuild table [c1, c4]
--echo # ------------------------------------------------------------
ALTER TABLE t1 FORCE;

--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
SELECT * FROM t1 ORDER BY c1;

INSERT INTO t1 values ("row8_c1", "row8_c4");
SELECT * FROM t1 ORDER BY c1;

if ($is_debug)
{
  DROP TABLE t1;
  --echo # ------------------------------------------------------------
  --echo # ONLINE DDL on table having INSTANT ADD/DROP Column
  --echo # ------------------------------------------------------------
  --echo
  --echo # ---------------------------
  --echo # ONLINE DDL 1 : NO PK CHANGE
  --echo # ---------------------------
  --source include/count_sessions.inc

  --eval CREATE TABLE t1 (c1 CHAR(10), c2 CHAR(10)) ROW_FORMAT=$row_format
  INSERT INTO t1 VALUES ("r1c1", "r1c2");
  SELECT * FROM t1;

  ALTER TABLE t1 ADD COLUMN c0 CHAR(10) DEFAULT "def_c0" FIRST, ALGORITHM=INSTANT;
  INSERT INTO t1 VALUES ("r2c0", "r2c1", "r2c2");
  SELECT * FROM t1;

  ALTER TABLE t1 DROP COLUMN c1, ALGORITHM=INSTANT;
  INSERT INTO t1 VALUES ("r3c0", "r3c2");
  SELECT * FROM t1;


  --echo # Make alter table wait
  SET DEBUG_SYNC = 'row_log_table_apply1_before SIGNAL s1 WAIT_FOR s2';

  --echo # Rebuild the table
  --send ALTER TABLE t1 FORCE, ALGORITHM=INPLACE

  connect (con1,localhost,root,,);
  --echo # connection con1
  connection con1;

  SET DEBUG_SYNC = 'now WAIT_FOR s1';

  --echo # Insert query
  INSERT INTO t1 VALUES ("r4c0", "r4c2");

  --echo # Update query
  --echo # (NOT INPLACE update for row1, IN-PLACE update for row2 an row3)
  UPDATE t1 SET c0="c0_upd";

  --echo # Let Alter table continue
  SET DEBUG_SYNC = 'now SIGNAL s2';

  --echo # connection default
  connection default;
  --reap

  SELECT * FROM t1;

  DISCONNECT con1;

  # Wait till all disconnects are completed
  --source include/wait_until_count_sessions.inc
  DROP TABLE t1;

  --echo # ---------------------------
  --echo # ONLINE DDL 2 : PK CHANGE
  --echo # ---------------------------
  --source include/count_sessions.inc

  --eval CREATE TABLE t1 (c1 CHAR(10), c2 CHAR(10)) ROW_FORMAT=$row_format
  INSERT INTO t1 VALUES ("r1c1", "r1c2");
  SELECT * FROM t1;

  ALTER TABLE t1 ADD COLUMN c0 CHAR(10) DEFAULT "def_c0" FIRST, ALGORITHM=INSTANT;
  INSERT INTO t1 VALUES ("r2c0", "r2c1", "r2c2");
  SELECT * FROM t1;

  ALTER TABLE t1 DROP COLUMN c1, ALGORITHM=INSTANT;
  INSERT INTO t1 VALUES ("r3c0", "r3c2");
  SELECT * FROM t1;


  --echo # Make alter table wait
  SET DEBUG_SYNC = 'row_log_table_apply1_before SIGNAL s1 WAIT_FOR s2';

  --echo # Rebuild the table
  --send ALTER TABLE t1 ADD PRIMARY KEY (c2), ALGORITHM=INPLACE

  connect (con1,localhost,root,,);
  --echo # connection con1
  connection con1;

  SET DEBUG_SYNC = 'now WAIT_FOR s1';

  --echo # Insert query
  INSERT INTO t1 VALUES ("r4c0", "r4c2");

  --echo # Update query
  --echo # (NOT INPLACE update for row1, IN-PLACE update for row2 an row3)
  UPDATE t1 SET c0="c0_upd";

  --echo # Let Alter table continue
  SET DEBUG_SYNC = 'now SIGNAL s2';

  --echo # connection default
  connection default;
  --reap

  SELECT * FROM t1;

  DISCONNECT con1;

  # Wait till all disconnects are completed
  --source include/wait_until_count_sessions.inc
  DROP TABLE t1;

  --echo # -----------------------
  --echo # ONLINE DDL 3 :
  --echo # -----------------------
  --source include/count_sessions.inc
  
  --eval create table t1 (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format
  Insert into t1 values ("r1c1", "r1c2");
  Select * from t1;

  alter table t1 add column c3 char(10) default "c3_def", algorithm=instant;
  Select * from t1;

  Insert into t1 values ("r2c1", "r2c2", "r2c3");
  Select * from t1;

  Alter table t1 add column c4 char(10) default "c4_def" first, algorithm=INSTANT;
  Select * from t1;

  Insert into t1 values ("r3c4", "r3c1", "r3c2", "r3c3");
  Select * from t1;

  --echo # Make alter table wait
  SET DEBUG_SYNC = 'row_log_table_apply1_before SIGNAL s1 WAIT_FOR s2';

  --echo # Rebuild the table
  --send_eval ALTER TABLE t1 force, algorithm=inplace;

  connect (con1,localhost,root,,);
  --echo # connection con1
  connection con1;

  SET DEBUG_SYNC = 'now WAIT_FOR s1';

  --echo # Update query
  eval UPDATE t1 SET c1="c1_upd";

  --echo # Let Alter table continue
  SET DEBUG_SYNC = 'now SIGNAL s2';

  --echo # connection default
  connection default;
  --reap

  SELECT * FROM t1;

  DISCONNECT con1;

  # Wait till all disconnects are completed
  --source include/wait_until_count_sessions.inc
}

--echo ###########
--echo # CLEANUP #
--echo ###########
DROP TABLE t1;
