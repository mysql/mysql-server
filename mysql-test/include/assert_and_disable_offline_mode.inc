# ==== Purpose ====
#
# Assert that offline_mode variable is enabled and reset its value.
#
# ==== Usage ====
#
# [--let $skip_reset_offline_mode= 1]
# [--let $rpl_debug= 1]
# --source include/assert_and_disable_offline_mode.inc
#
# Parameters:
#   $skip_reset_offline_mode
#     Skip reset offline_mode variable.
#
#   $rpl_debug
#     See include/rpl/init.inc

--let $include_filename= assert_and_disable_offline_mode.inc
--source include/begin_include_file.inc

if (!$rpl_debug)
{
  --disable_query_log
}

--let $wait_condition= SELECT @@GLOBAL.offline_mode = 1
--source include/wait_condition_or_abort.inc

if (!$skip_reset_offline_mode)
{
  SET @@GLOBAL.offline_mode = OFF;
}
--let $skip_reset_offline_mode=

--let $include_filename= assert_and_disable_offline_mode.inc
--source include/end_include_file.inc
