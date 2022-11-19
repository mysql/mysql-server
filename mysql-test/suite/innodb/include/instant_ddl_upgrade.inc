##########################################################################
# Test script to test UPGRADE from older version table (not) having
# INSTANT ADD columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--echo # ------------------------------------------------------------
--echo # Start server with old datadir with
--echo #   table t1_$row_format having
--echo #     c1, c2 normal columns
--echo #     c3 INSTANT ADD column with default def_c3.
--echo #     c4 INSTANT ADD column with default def_c4.
--echo #       +------+------+--------+--------+
--echo #       | c1   | c2   | c3     | c4     |
--echo #       +------+------+--------+--------+
--echo #       | r1c1 | r1c2 | c3_def | c4_def |
--echo #       | r2c1 | r2c2 | r2c3   | c4_def |
--echo #       | r3c1 | r3c2 | r3c3   | r3c4   |
--echo #       +------+------+--------+--------+
--echo #
--echo #   table t2_$row_format having
--echo #     c1, c2 normal columns
--echo #     c3 INSTANT ADD column with default def_c3.
--echo #       +------+------+--------+
--echo #       | c1   | c2   | c3     |
--echo #       +------+------+--------+
--echo #       | r1c1 | r1c2 | c3_def |
--echo #       | r2c1 | r2c2 | r2c3   |
--echo #       +------+------+--------+
--echo # ------------------------------------------------------------

eval SHOW CREATE TABLE t1_$row_format;

--echo # ------------------------------------------------------------
--echo # Read rows from upgraded table t1_$row_format
--echo # ------------------------------------------------------------
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_metadata.inc
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Insert a row in upgraded table t1_$row_format
--echo # ------------------------------------------------------------
eval INSERT INTO t1_$row_format values ("r4c1", "r4c2", "r4c3", "r4c4");
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # UPDATE : IN PLACE for row with no version
--echo # ------------------------------------------------------------
eval UPDATE t1_$row_format SET c2="temp" where c1="r1c1";
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # UPDATE : IN PLACE for row with V1
--echo # ------------------------------------------------------------
eval UPDATE t1_$row_format SET c2="temp" where c1="r2c1";
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # UPDATE : NOT IN PLACE for row with no version
--echo # ------------------------------------------------------------
eval UPDATE t1_$row_format SET c3="r1c3" where c1="r1c1";
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # UPDATE : NOT IN PLACE for row with V1
--echo # ------------------------------------------------------------
eval UPDATE t1_$row_format SET c4="r2c4" where c1="r2c1";
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # INSTANT DROP c2
--echo # ------------------------------------------------------------
eval ALTER TABLE t1_$row_format DROP COLUMN c2, ALGORITHM=INSTANT;
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_metadata.inc
eval SELECT * FROM t1_$row_format ORDER BY c1;
eval INSERT INTO t1_$row_format values ("r5c1", "r5c3", "r5c4");
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # INSTANT ADD c5 at the end
--echo # ------------------------------------------------------------
eval ALTER TABLE t1_$row_format ADD COLUMN c5 char(10) default "c5_def", ALGORITHM=INSTANT;
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_metadata.inc
eval SELECT * FROM t1_$row_format ORDER BY c1;
eval INSERT INTO t1_$row_format values ("r6c1", "r6c3", "r6c4", "r6c5");
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # INSTANT ADD c6 somewhere in between
--echo # ------------------------------------------------------------
eval ALTER TABLE t1_$row_format ADD COLUMN c6 char(10) default "c6_def" after c1, ALGORITHM=INSTANT;
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_metadata.inc
eval SELECT * FROM t1_$row_format ORDER BY c1;
eval INSERT INTO t1_$row_format values ("r7c1", "r7c6", "r7c3", "r7c4", "r7c5");
eval SELECT * FROM t1_$row_format ORDER BY c1;

eval DROP TABLE t1_$row_format;

if ($is_debug)
{
  --echo # ------------------------------------------------------------
  --echo # TEMP record with different possibilities
  --echo # ------------------------------------------------------------
  --source include/count_sessions.inc

  eval show create table t2_$row_format;

  eval Select * from t2_$row_format;
  eval Insert into t2_$row_format values ("r3c1", "r3c2", "r3c3");
  eval Select * from t2_$row_format;
  eval Alter table t2_$row_format add column c4 char(10) default "c4_def" first, algorithm=INSTANT;
  eval Insert into t2_$row_format values ("r4c4", "r4c1", "r4c2", "r4c3");
  eval Select * from t2_$row_format;

  --echo # Make alter table wait
  SET DEBUG_SYNC = 'row_log_table_apply1_before SIGNAL s1 WAIT_FOR s2';

  --echo # Rebuild the table
  --send_eval ALTER TABLE t2_$row_format force, algorithm=inplace;

  connect (con1,localhost,root,,);
  --echo # connection con1
  connection con1;

  SET DEBUG_SYNC = 'now WAIT_FOR s1';

  --echo # Update query
  eval UPDATE t2_$row_format SET c1="c1_upd";

  --echo # Let Alter table continue
  SET DEBUG_SYNC = 'now SIGNAL s2';

  --echo # connection default
  connection default;
  --reap

  eval SELECT * FROM t2_$row_format;

  DISCONNECT con1;

  # Wait till all disconnects are completed
  --source include/wait_until_count_sessions.inc

  eval DROP TABLE t2_$row_format;
}
