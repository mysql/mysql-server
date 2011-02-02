# ==== Purpose ====
#
# This file does the same as the built-in command sync_with_master,
# but can be configured to use a custom timeout.  This has the benefit
# that it accepts the same $slave_timeout and $master_connection
# parameters as wait_for_slave_param.inc
#
#
# ==== Usage ====
#
# --connection master
# --source include/save_master_pos.inc
# --connection slave
# --source include/sync_with_master.inc
#
# Parameters to this macro are $slave_timeout and
# $master_connection. See wait_for_slave_param.inc for
# descriptions.

--let $slave_param= Relay_Master_Log_File
--let $slave_param_value= $_master_file
--source include/wait_for_slave_param.inc

--let $slave_param= Exec_Master_Log_Pos
--let $slave_param_value= $_master_pos
--source include/wait_for_slave_param.inc
