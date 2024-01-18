# ==== Purpose ====
#
# Apply initial saved auto_increment_increment and auto_increment_offset value
# in store_group_replication_auto_increment.inc. The auto_increment values are
# stored in variables of format $server_auto_increment_increment_$server_number
# and $server_auto_increment_offset_$server_number, as for example for server1
# variables $server_auto_increment_offset_1 and
# $server_auto_increment_increment_1 will store auto_increment values.
#
# ==== Usage ====
#
# --let $rpl_server_number= NUMBER
# [--let $rpl_debug= 1]
# --source include/restore_group_replication_auto_increment.inc
#
# Parameters:
#   $rpl_server_number
#     server number for which to apply saved auto_increment_increment and
#     auto_increment_offset.
#
#   $rpl_debug
#     See include/rpl/init.inc

--let $include_filename= restore_group_replication_auto_increment.inc
--source include/begin_include_file.inc

if (!$rpl_server_number)
{
  --die ERROR IN TEST: You must set $rpl_server_number before sourcing include/restore_group_replication_auto_increment.inc
}

if (!$rpl_debug)
{
  --disable_query_log
}

# reset auto_increment_increment stored in $server_auto_increment_increment_$rpl_server_number
# in store_group_replication_auto_increment.inc
--let $tmp_auto_inc_val= \$server_auto_increment_increment_$rpl_server_number
if ($tmp_auto_inc_val)
{
  --eval SET GLOBAL auto_increment_increment= $tmp_auto_inc_val
}

# reset auto_increment_offset stored in $server_auto_increment_offset_$rpl_server_number
# in store_group_replication_auto_increment.inc
--let $tmp_auto_inc_val= \$server_auto_increment_offset_$rpl_server_number
if ($tmp_auto_inc_val)
{
  --eval SET GLOBAL auto_increment_offset= $tmp_auto_inc_val
}

if (!$rpl_debug)
{
  --enable_query_log
}

--let $include_filename= restore_group_replication_auto_increment.inc
--source include/end_include_file.inc
