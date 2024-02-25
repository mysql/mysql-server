# Compare locks taken for $statements in various contexts i.e
# When executed as a
# a. Normal statement inside a transaction.
# b. Substatement inside a trigger.
# c. Substatement inside a stored function.
# These operations are performed under all four isolation levels:
# READ UNCOMMITTED, READ COMMITTED, REPEATABLE READ, SERIALIZABLE

# Usage:
# Set $statements to the query you want to execute in the
# above three contexts and compare locks taken.
# We set $skip_lock_check parameter to 1 to skip testing behavior in
# REPEATABLE READ mode for some buggy cases which are not fixed yet.

call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");

--connect (con1, localhost, root,,)
# The main table, used for monitoring the locking strategy
CREATE TABLE t1 (
  id INT PRIMARY KEY,
  val varchar(4) NOT NULL,
  KEY idx_val (val)
) ENGINE=InnoDB;
# Preload the table with some data
INSERT INTO t1 VALUES (10, 'a'), (20, 'b');

# An additional table to enable scenarios with more complicated queries
CREATE TABLE t2 LIKE t1;
INSERT INTO t2 SELECT * FROM t1;

# A table to cause a TRIGGER to excecute $statements
CREATE TABLE t_triggerable (
  id INT PRIMARY KEY
) ENGINE=InnoDB;

DELIMITER //;
eval
CREATE TRIGGER bef_ins_t_triggerable BEFORE INSERT ON t_triggerable FOR EACH ROW
BEGIN
  $statements;
END;//

eval
CREATE FUNCTION statements_in_function() RETURNS INTEGER DETERMINISTIC
BEGIN
  $statements;
  RETURN 1;
END;//
DELIMITER ;//

# Helper table for lock comparison in regular case and trigger/function case.
CREATE TABLE stronger_or_equal(weaker VARCHAR(30), stronger VARCHAR(30));
INSERT INTO stronger_or_equal VALUES
("S", "S"),
("S", "X"),
("X", "X"),
("S,GAP", "S"),
("S,GAP", "X"),
("X,GAP", "X"),
("S,GAP", "S,GAP"),
("S,GAP", "X,GAP"),
("X,GAP", "X,GAP"),
("S,REC_NOT_GAP", "S"),
("S,REC_NOT_GAP", "X"),
("X,REC_NOT_GAP", "X"),
("S,REC_NOT_GAP", "S,REC_NOT_GAP"),
("S,REC_NOT_GAP", "X,REC_NOT_GAP"),
("X,REC_NOT_GAP", "X,REC_NOT_GAP"),
("X,INSERT_INTENTION", "X,INSERT_INTENTION"),
("X,GAP,INSERT_INTENTION", "X,GAP,INSERT_INTENTION");

--connect (con2, localhost, root,,)
--disable_warnings

--let $isolation_levels=READ UNCOMMITTED,READ COMMITTED,REPEATABLE READ,SERIALIZABLE
while ($isolation_levels)
{
  --let $isolation_level=`SELECT SUBSTRING_INDEX('$isolation_levels', ',', 1)`
  --let $isolation_levels= `SELECT SUBSTRING('$isolation_levels', LENGTH('$isolation_level') + 2)`

  --let $RUC_or_RC= `SELECT '$isolation_level'='READ UNCOMMITTED' OR '$isolation_level'='READ COMMITTED'`
  # InnoDB is limited to row-logging when transaction isolation level is READ COMMITTED or READ UNCOMMITTED.
  --let $incompatible_with_binlog_format= `SELECT '$binlog_format'='STATEMENT' AND '$RUC_or_RC'=1`

  if ( !$incompatible_with_binlog_format )
  {
    --eval SET SESSION TRANSACTION ISOLATION LEVEL $isolation_level
    --echo # Isolation level: $isolation_level

    --let $contexts=regular_case,trigger,function
    while ($contexts)
    {
      --let $context= `SELECT SUBSTRING_INDEX('$contexts', ',', 1)`
      --let $contexts= `SELECT SUBSTRING('$contexts', LENGTH('$context') + 2)`
      --let $skip_assert= 0;

      BEGIN;
      if ( $context == "regular_case" ) {
        --eval $statements
      }
      if ( $context == "trigger" ) {
        INSERT INTO t_triggerable VALUES (3);
      }
      if ( $context == "function" ) {
        --disable_result_log
        SELECT statements_in_function();
        --enable_result_log

        if ( $isolation_level == "REPEATABLE READ" ) {
          if ( $skip_lock_check ) {
            --let $skip_assert= 1
          }
        }
      }

      --connection con1
        --let $need_locks_data=1
        while ($need_locks_data) {
          eval CREATE TABLE locks_in_$context
            SELECT LOCK_MODE, LOCK_DATA, LOCK_TYPE
            FROM performance_schema.data_locks
            WHERE OBJECT_NAME='t1';

          --let $need_locks_data=0
          if (`SELECT COUNT(*) FROM locks_in_$context WHERE LOCK_DATA IS NULL AND LOCK_TYPE="RECORD"`) {
            # Sporadically, P_S can not access record's page in buffer pool and reconstruct LOCK_DATA values
            --eval DROP TABLE locks_in_$context
            --let $need_locks_data=1
            # wait a little to not cause furious spinning and allow for release of latches on page, if any
            --sleep 1
            # Try to bring back the pages into buffer pool using a non-locking SELECT, if they were missing
            --disable_result_log
            SELECT * FROM t1;
            --enable_result_log
          }
        }

      --connection con2
        ROLLBACK;

      if ( $context != "regular_case" ) {

        if ( $RUC_or_RC ) {
          --echo # Comparing locks taken for the above query in regular case and when inside a $context.
          --echo # Locks taken in $context should always be same as that in regular case.
          --let $diff_tables = locks_in_regular_case, locks_in_$context
          --source include/diff_tables.inc
        }
        if ( !$RUC_or_RC ) {
		  # Unlike for READ UNCOMMITTED/COMMITTED modes the locks taken inside of stored
          # function/trigger in REPEATABLE READ/SERIALIZABLE modes can be stronger than
          # those taken in regular case. This is due to fact that we support SBR
          # in these isolation modes. In case of SBR, statement which is run in regular case
          # and is not logged can use relaxed locking, while the same statement run from
          # trigger/stored function will be always "logged" due to logging of triggering
          # statement or function invocation and thus require stricter locks.
          if ( !$skip_assert ) {
            --echo # Comparing locks taken for the above query in regular case and when inside a $context.
            let $assert_text= Locks taken in $context should always be stronger or equal to locks taken in regular case;
            let $assert_cond= "
              [SELECT count(*) FROM locks_in_regular_case LEFT JOIN
                (SELECT * FROM stronger_or_equal JOIN locks_in_$context
                 ON locks_in_$context.LOCK_MODE = stronger WHERE locks_in_$context.LOCK_TYPE="RECORD") `st_or_eq`
               ON locks_in_regular_case.LOCK_MODE = weaker AND locks_in_regular_case.LOCK_DATA = st_or_eq.LOCK_DATA
               WHERE locks_in_regular_case.LOCK_TYPE="RECORD" AND st_or_eq.LOCK_MODE IS NULL]" = 0;
            source include/assert.inc;
          }
        }
        --echo
        eval DROP TABLE locks_in_$context;
      }
    }
    DROP TABLE locks_in_regular_case;
  }
}

--enable_warnings
--disconnect con2
--source include/wait_until_disconnected.inc
--disconnect con1
--source include/wait_until_disconnected.inc
--disable_query_log

--connection default
DROP TABLE stronger_or_equal;
DROP FUNCTION statements_in_function;
DROP TABLE t_triggerable;
DROP TABLE t1;
DROP TABLE t2;
