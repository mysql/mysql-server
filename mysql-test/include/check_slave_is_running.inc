# ==== Purpose ====
#
# Assert that the slave threads are running and don't have any errors.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# [--let $rpl_debug= 1]
# --source include/check_slave_is_running.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/check_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= check_slave_is_running.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


--let $slave_param= Slave_IO_Running
--let $slave_param_value= Yes
--source include/check_slave_param.inc

--let $slave_param= Slave_SQL_Running
--let $slave_param_value= Yes
--source include/check_slave_param.inc


--let $include_filename= check_slave_is_running.inc
--source include/end_include_file.inc
