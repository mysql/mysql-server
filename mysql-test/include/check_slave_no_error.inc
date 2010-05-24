# ==== Purpose ====
#
# Assert that Slave_SQL_Error and Slave_IO_Error are empty.
#
# ==== Usage ====
#
# --let $slave_param= Exec_Master_Log_Pos
# --let $slave_param_value= 4711
# --source include/check_slave_running.inc

--let $slave_param= Last_SQL_Errno
--let $slave_param_value= 0
--source include/check_slave_param.inc

--let $slave_param= Last_IO_Errno
--let $slave_param_value= 0
--source include/check_slave_param.inc
