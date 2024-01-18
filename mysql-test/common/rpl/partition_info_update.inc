# ==== Purpose ====
#
# Assert that the binary log contains a specific sequence of partition ids
# for UPDATE Events (having both a partition id for the
# before-image($source_partition_id), and one for the after-image(target_partition_id)).
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
#   $partition_id
#     This is a semicolon-separated list of pairs of partition_id.
#     Each pair consists of 'source_partition_id' as an integer
#     followed by space followed by 'target_partition_id' as an integar.
#     Note that this is whitespace-sensitive so there should
#     not be any other space characters than those separating
#     source_partition_id from target_partition_id.
#  ==== References ====
#
# Bug#28977199:UPDATE EVENT LOGS ONLY THE TARGET PARTITION WHEN MODIFYING MULTIPLE PARTITION
# WL#12168:Add Partition Information into the Binary Log


if ($partition_id == '')
{
  --die !!!ERROR IN TEST: you must set $target_partition_id
}

--let $include_filename= rpl/partition_info_update.inc
--source include/begin_include_file.inc

--exec $MYSQL_BINLOG --start-position=$binlog_position -vv $binlog_fullpath > $MYSQLTEST_VARDIR/tmp/mysqlbinlog_partition.binlog
--let $assert_file = $MYSQLTEST_VARDIR/tmp/mysqlbinlog_partition.binlog

--let $assert_select= source_partition:.*target_partition:.*
--let $assert_match= `SELECT CONCAT('source_partition: ', REPLACE(REPLACE('$partition_id', ' ', '.*target_partition: '),';', '.*\n[^\n]*source_partition: '))`
--let $assert_text= assert_partition_info: $partition_id
--source include/assert_grep.inc

--let $partition_id=
--remove_file $MYSQLTEST_VARDIR/tmp/mysqlbinlog_partition.binlog
--let $include_filename= rpl/partition_info_update.inc
--source include/end_include_file.inc


