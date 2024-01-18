# ==== Purpose ====
#
# Assert that the binary log contains a specific sequence of
# partition ids for INSERT and DELETE statements
#
# ==== Usage ====
#
# --let $binlog_fullpath= TEXT
# --let $binlog_position= NUMBER
# --let $partition_id= NUMBER
# --source include/rpl/partition_info.inc
#
# Parameters:
#
#   $binlog_fullpath
#     The full path of binary log to look in.
#     (settable using include/rpl/save_binlog_file_position.inc)
#   $binlog_position
#     The log position to start reading from
#     (settable using include/rpl/save_binlog_file_position.inc)
#   $expected_partition_id
#     The partition_id where the row changes are done. It will contain
#     a single value or a semicolon-separated list of partition_id.
#  ==== References ====
#
# Bug#28712618:INSERT/UPDATE/DELETE WITH MULTIPLE ROWS IN SAME PARTITION CREATE MULTIPLE EVENT
# WL#12168:Add Partition Information into the Binary Log

if ($expected_partition_id == '')
{
  --die !!!ERROR IN TEST: you must set $expected_partition_id
}

--let $include_filename= rpl/partition_info.inc
--source include/begin_include_file.inc

--exec $MYSQL_BINLOG --start-position=$binlog_position -vv $binlog_fullpath > $MYSQLTEST_VARDIR/tmp/mysqlbinlog_partition.binlog
--let $assert_file = $MYSQLTEST_VARDIR/tmp/mysqlbinlog_partition.binlog

--let $assert_select= partition:
--let $assert_match= `SELECT CONCAT('partition: ', REPLACE('$expected_partition_id', ';', '.*\n[^\n]*partition: '))`
--let $assert_text= assert_partition_id: $expected_partition_id
--source include/assert_grep.inc

--let $expected_partition_id=
--remove_file $MYSQLTEST_VARDIR/tmp/mysqlbinlog_partition.binlog
--let $include_filename= rpl/partition_info.inc
--source include/end_include_file.inc
