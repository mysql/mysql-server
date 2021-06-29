# ==== Purpose ====
#
# This include will check if the optimized delete (delete_all_rows) was
# used or not varying the session sql_log_bin variable with 0 and 1.
#
# It is expected that the optimized delete (delete_all_rows) will be used:
# a) If binlog_format != ROW
# b) If binlog_format == ROW but the statement will not be binlogged
#
# The include, varying sql_log_bin with 0 and 1, creates a new table (t1),
# fills it with data, and deletes all t1 data with a single query.
#
# It uses an EXPLAIN to assert if delete_all_rows was used or not.
#
# The include also asserts if the statement was binlogged if it is expected
# to be.
#
# ==== Usage ====
#
# [--let $storage_engine= InnoDB | MyISAM]
# --source include/delete_all_rows.inc
#
# Parameters:
#   $storage_engine
#     The storage engine that will be used in the CREATE TABLE statements.
#     If not specified, InnoDB will be used.
#

if (!$storage_engine)
{
  --let $_storage_engine= InnoDB
}
if ($storage_engine)
{
  --let $_storage_engine= $storage_engine
}

##
## Determine if the server has the binary log enabled
##
--let $have_log_bin= query_get_value(SHOW GLOBAL VARIABLES LIKE 'log_bin', Value, 1)

# Create t1 and populate it
--eval CREATE TABLE t1 (c1 INT) ENGINE=$_storage_engine
INSERT INTO t1 VALUES (1), (2), (3);

##
## Session sql_log_bin = 0
##
--echo
SET sql_log_bin= 0;

# Assert that delete_all_rows() will be used
# Force use of the old optimizer, since the hypergraph optimizer neither
# supports the traditional EXPLAIN format, nor uses delete_all_rows.
let $explain_extra = query_get_value(
  "EXPLAIN DELETE /*+ set_var(optimizer_switch='hypergraph_optimizer=off') */
   FROM t1", Extra, 1);
--let $assert_text= DELETE will use delete_all_rows
--let $assert_cond= "$explain_extra" = "Deleting all rows"
--source include/assert.inc

# Save current master position
if ($have_log_bin == ON)
{
  --let $saved_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
}
DELETE FROM t1;

# Assert that this was not binlogged if binary log is enabled
if ($have_log_bin == ON)
{
  --let $current_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
  --let $assert_text= DELETE was not binlogged
  --let $assert_cond= $current_pos = $saved_pos
  --source include/assert.inc
}

##
## Session sql_log_bin = 1
##
--echo
SET sql_log_bin= 1;
# Populate t1 again
INSERT INTO t1 VALUES (1), (2), (3);

# Assert if delete_all_rows() will be used or not
--let $explain_extra_expected= "Deleting all rows"
# The extra info in explain will be NULL if the statement is binlogged
# with binlog_format = ROW
# Force use of the old optimizer, since the hypergraph optimizer neither
# supports the traditional EXPLAIN format, nor uses delete_all_rows.
let $explain_extra = query_get_value(
  "EXPLAIN DELETE /*+ set_var(optimizer_switch='hypergraph_optimizer=off') */
   FROM t1", Extra, 1);
--let $assert_text= DELETE will use delete_all_rows
if ($have_log_bin == ON)
{
  if (`SELECT @@GLOBAL.binlog_format = 'ROW'`)
  {
    --let $assert_text= DELETE will not use delete_all_rows
    --let $explain_extra_expected= "NULL"
  }
}
--let $assert_cond= "$explain_extra" = $explain_extra_expected
--source include/assert.inc

# Save current master position
if ($have_log_bin == ON)
{
  --let $saved_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
}

DELETE FROM t1;
# Assert that this was binlogged if binary log is enabled
if ($have_log_bin == ON)
{
  --let $current_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
  --let $assert_text= DELETE was binlogged
  --let $assert_cond= $current_pos > $saved_pos
  --source include/assert.inc
}

# Cleanup
DROP TABLE t1;
