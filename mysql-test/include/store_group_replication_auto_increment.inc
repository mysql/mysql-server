# ==== Purpose ====
#
# Save initial auto_increment_increment and auto_increment_offset values
#
# ==== Usage ====
#
# --let $rpl_server_number= NUMBER
# [--let $rpl_debug= 1]
# --source include/store_group_replication_auto_increment.inc
#
# Parameters:
#   $rpl_server_number
#     server number for which to save auto_increment_increment and
#     auto_increment_offset. This number will be used to reference
#     all the auto_increment values at the end to fetch and reset.
#
#   $rpl_debug
#     See include/rpl_init.inc

--let $include_filename= store_group_replication_auto_increment.inc
--source include/begin_include_file.inc

if (!$rpl_server_number)
{
  --die ERROR IN TEST: You must set $rpl_server_number before sourcing include/store_group_replication_auto_increment.inc
}

if (!$rpl_debug)
{
  --disable_query_log
}

--let $_tmp_auto_increment_increment= `SELECT @@GLOBAL.AUTO_INCREMENT_INCREMENT`
--let $_tmp_auto_increment_offset= `SELECT @@GLOBAL.AUTO_INCREMENT_OFFSET`

if ($rpl_server_number == 1) {
  --let $server_auto_increment_increment_1= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_1= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 2) {
  --let $server_auto_increment_increment_2= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_2= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 3) {
  --let $server_auto_increment_increment_3= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_3= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 4) {
  --let $server_auto_increment_increment_4= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_4= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 5) {
  --let $server_auto_increment_increment_5= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_5= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 6) {
  --let $server_auto_increment_increment_6= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_6= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 7) {
  --let $server_auto_increment_increment_7= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_7= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 8) {
  --let $server_auto_increment_increment_8= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_8= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 9) {
  --let $server_auto_increment_increment_9= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_9= $_tmp_auto_increment_offset
}

if ($rpl_server_number == 10) {
  --let $server_auto_increment_increment_10= $_tmp_auto_increment_increment
  --let $server_auto_increment_offset_10= $_tmp_auto_increment_offset
}


if (!$rpl_debug)
{
  --enable_query_log
}

--let $include_filename= store_group_replication_auto_increment.inc
--source include/end_include_file.inc
