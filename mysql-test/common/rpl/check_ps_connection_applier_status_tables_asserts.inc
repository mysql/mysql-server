# This file is used by common/rpl/check_ps_connection_applier_status_tables.inc
#
# Using debug points to track the execution flow of a transaction in the slave,
# this file asserts that the new columns in the queried tables have the
# expected values.
#
# params: $current_gtid               the gtid that is currently being processed
#         $last_gtid                  the gtid that was last processed
#         $debug_sync_[1-4]           the names of the four debug sync points
#         $is_first_check             1 if is first table to be queried
#         $table                      table to be queried
#         $now_handling_gtid_column   name of the column for now handling GTID
#         $now_handling_OCT_column    name of the column for now handling OCT
#         $now_handling_ICT_column    name of the column for now handling ICT
#         $now_handling_start_column  name of the column for now handling start
#         $last_handled_gtid_column   name of column for last handled GTID
#         $last_handled_OCT_column    name of column for last handled OCT
#         $last_handled_ICT_column    name of column for last handled ICT
#         $last_handled_start_column  name of column for last handled start
#         $last_handled_end_column    name of column for last handled end
#         $now_handling_action        trx action this table refers to
#         $last_handled_action        trx action this table refers to
#         $is_first_stmt              1 is this is the first statement of the
#                                     test

--let $gtid_ts_pfs_enabled=`SELECT @@GLOBAL.performance_schema`

eval SET debug_sync= $debug_sync_1;
if ($is_first_check)
{
 --source include/remove_debug_point.inc
}

--let $now_handling_before_gtid= query_get_value(SELECT $now_handling_gtid_column FROM $table, $now_handling_gtid_column, 1)
--let $now_handling_before_OCT= query_get_value(SELECT $now_handling_OCT_column FROM $table, $now_handling_OCT_column, 1)
--let $now_handling_before_ICT= query_get_value(SELECT $now_handling_ICT_column FROM $table, $now_handling_ICT_column, 1)
--let $now_handling_before_start= query_get_value(SELECT $now_handling_start_column FROM $table, $now_handling_start_column, 1)
--let $last_handled_before_gtid= query_get_value(SELECT $last_handled_gtid_column FROM $table, $last_handled_gtid_column, 1)
eval SET debug_sync= $debug_sync_2;

eval SET debug_sync= $debug_sync_3;
--let $now_handling_after_gtid= query_get_value(SELECT $now_handling_gtid_column FROM $table, $now_handling_gtid_column, 1)
--let $now_handling_after_OCT= query_get_value(SELECT $now_handling_OCT_column FROM $table, $now_handling_OCT_column, 1)
--let $now_handling_after_ICT= query_get_value(SELECT $now_handling_ICT_column FROM $table, $now_handling_ICT_column, 1)
--let $now_handling_after_start= query_get_value(SELECT $now_handling_start_column FROM $table, $now_handling_start_column, 1)
--let $last_handled_after_gtid= query_get_value(SELECT $last_handled_gtid_column FROM $table, $last_handled_gtid_column, 1)
--let $last_handled_after_OCT= query_get_value(SELECT $last_handled_OCT_column FROM $table, $last_handled_OCT_column, 1)
--let $last_handled_after_ICT= query_get_value(SELECT $last_handled_ICT_column FROM $table, $last_handled_ICT_column, 1)
--let $last_handled_after_start= query_get_value(SELECT $last_handled_start_column FROM $table, $last_handled_start_column, 1)
--let $last_handled_after_end= query_get_value(SELECT $last_handled_end_column FROM $table, $last_handled_end_column, 1)
eval SET debug_sync= $debug_sync_4;

--let $assert_text= Assert that the currently $now_handling_action gtid is equal to current_gtid
--let $assert_cond= "$now_handling_before_gtid" = "$current_gtid"
--source include/assert.inc

--let $assert_text= Assert that the last $last_handled_action gtid is equal to last_gtid
--let $assert_cond= "$last_handled_before_gtid" = "$last_gtid"
--source include/assert.inc

--let $assert_text= Assert that currently $now_handling_action gtid was cleared
--let $assert_cond= "$now_handling_after_gtid" = ""
--source include/assert.inc

--let $assert_text= Assert that currently $now_handling_action OCT was cleared
--let $assert_cond= "$now_handling_after_OCT" = "0000-00-00 00:00:00.000000"
--source include/assert.inc

--let $assert_text= Assert that currently $now_handling_action ICT was cleared
--let $assert_cond= "$now_handling_after_ICT" = "0000-00-00 00:00:00.000000"
--source include/assert.inc

--let $assert_text= Assert that currently $now_handling_action start was cleared
--let $assert_cond= "$now_handling_after_start" = "0000-00-00 00:00:00.000000"
--source include/assert.inc

--let $assert_text= Assert that the last $last_handled_action gtid is equal to current_gtid
--let $assert_cond= "$last_handled_after_gtid" = "$current_gtid"
--source include/assert.inc

--let $assert_text= Assert that currently $now_handling_action OCT exists
--let $assert_cond= "$now_handling_before_OCT" != "0000-00-00 00:00:00.000000"
--source include/assert.inc

--let $assert_text= Assert that currently $now_handling_action ICT exists
--let $assert_cond= "$now_handling_before_ICT" != "0000-00-00 00:00:00.000000"
--source include/assert.inc

if ($gtid_ts_pfs_enabled)
{
  --let $assert_text= Assert that currently $now_handling_action start_time exists
  --let $assert_cond= "$now_handling_before_start" != "0000-00-00 00:00:00.000000"
}
if (!$gtid_ts_pfs_enabled)
{
  --let $assert_text= Assert that currently $now_handling_action start_time was cleared
  --let $assert_cond= "$now_handling_before_start" = "0000-00-00 00:00:00.000000"
}
--source include/assert.inc

--let $assert_text= Assert that the OCT of the last $last_handled_action and currently $now_handling_action is the same
--let $assert_cond= "$now_handling_before_OCT" = "$last_handled_after_OCT"
--source include/assert.inc

--let $assert_text= Assert that the ICT of the last $last_handled_action and currently $now_handling_action is the same
--let $assert_cond= "$now_handling_before_ICT" = "$last_handled_after_ICT"
--source include/assert.inc

--let $assert_text= Assert that the start time of the last $last_handled_action and currently $now_handling_action is the same
--let $assert_cond= "$now_handling_before_start" = "$last_handled_after_start"
--source include/assert.inc

if ($gtid_ts_pfs_enabled)
{
  --let $unix_ts_end= `SELECT UNIX_TIMESTAMP('$last_handled_after_end')`
  --let $unix_ts_start= `SELECT UNIX_TIMESTAMP('$last_handled_after_start')`
  --let $assert_text= Assert that the end time of the last $last_handled_action is greater than the start time
  --let $assert_cond= $unix_ts_end - $unix_ts_start  > 0
  --source include/assert.inc
}
if (!$gtid_ts_pfs_enabled)
{
  --let $assert_text= Assert that the end time of the last $last_handled_action was cleared
  --let $assert_cond= "$last_handled_after_end" = "0000-00-00 00:00:00.000000"
  --source include/assert.inc
}
