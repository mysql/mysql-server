# ==== Purpose ====
#
# This include will insert some data into a table at the master after setting
# a debug sync point at the slave that should make the IO thread to stop in the
# middle of the transaction event stream (trying to let partial transactions in
# the relay log).
#
# After ensuring that the IO thread has stopped, this include will insert more
# data into the master table while the IO thread is stopped.
#
# Then, this include will start the slave IO thread again and wait for it to be
# synchronized with the master.
#
# If the SQL thread was running at the beginning of the include, it will sync
# the SQL thread after the insert/stop IO thread/insert sequence.
#
# ==== Usage ====
#
# --let $table= table_name
# --let $debug_point= debug_point
# [--let $counter= counter]
# [--let $info_table= info_table_name]
# [--let $gtids_after_stop= gtids after IO thread stopped]
# [--let $gtids_after_sync= gtids after sync IO/SQL thread]
# --source extra/rpl_tests/rpl_trx_boundary_parser_one_step.inc
#
# Parameters:
#   $table
#     The name of the table to insert data into.
#
#   $counter
#     The data that will be used in the INSERT statements.
#     If not specified, 0 will be used.
#
#   $debug_point
#     The debug point to be activated.
#
#   $info_table
#     The name of the table to insert information.
#     This table must have a column named "info" of VARCHAR(64) data type.
#

if (!$table)
{
  --die ERROR IN TEST: the mysqltest variable 'table' must be set
}
if (!$debug_point)
{
  --die ERROR IN TEST: the mysqltest variable 'debug_point' must be set
}
if (!$counter)
{
  --let $_counter= 0
}
if ($counter)
{
  --let $_counter= $counter
}

# Check if SQL thread is running
--source include/rpl_connection_slave.inc
--let $_is_sql_thread_running= query_get_value(SHOW SLAVE STATUS, Slave_SQL_Running, 1)

if ($info_table)
{
  # Inserting the debug point text into the info table
  --source include/rpl_connection_master.inc
  --eval INSERT INTO $info_table (info) VALUES ('$debug_point')
  # Make sure the debug_point will not be used by this INSERT
  --source include/sync_slave_io_with_master.inc
}

# Setting the debug sync point to stop IO thread
--let $debug_type= GLOBAL
--source include/add_debug_point.inc

# Inserting some data into master table $table
--source include/rpl_connection_master.inc
BEGIN;
--eval INSERT INTO $table VALUES (100 * $_counter + 1)
--eval SET @val= 100 * $_counter + 2
--eval INSERT INTO $table VALUES (@val)
COMMIT;

# Waiting for replica IO thread to stop
--source include/rpl_connection_slave.inc
--source include/wait_for_slave_io_to_stop.inc

--source include/remove_debug_point.inc

# Check if the IO thread retrieved the correct amount of GTIDs
--let $gtid_step_count= $gtids_after_stop
# Ensure that the SQL thread is synced with the IO thread
# before asserting using slave's the GTID_EXECUTED.
if (!$assert_on_retrieved_gtid_set)
{
  --source include/sync_slave_sql_with_io.inc
}
--source $gtid_step_assert_include

# Inserting more data into master table $table
--source include/rpl_connection_master.inc
--eval INSERT INTO $table VALUES (100 * $_counter + 3)

# Restating slave IO thread
--source include/rpl_connection_slave.inc
--source include/start_slave_io.inc

# Check if SQL thread was running before to sync it
if ( $_is_sql_thread_running == Yes )
{
  # Sync SQL thread
  --source include/rpl_connection_master.inc
  --source include/sync_slave_sql_with_master.inc
}
# Else we sync only the IO thread
if ( $_is_sql_thread_running == No )
{
  # Sync IO thread
  --source include/rpl_connection_master.inc
  --source include/sync_slave_io_with_master.inc
}

# Check if the IO thread retrieved the correct amount of GTIDs
--let $gtid_step_count= $gtids_after_sync
--source $gtid_step_assert_include
