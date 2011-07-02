# ==== Purpose ====
#
# Waits until the slave IO thread on the current connection has been
# synced up to the point saved by the last call to
# include/save_master_pos.inc (i.e., until the IO thead has copied up
# to the saved position).  Does not wait for the SQL thread.
#
#
# ==== Usage ====
#
# On master:
# --source include/save_master_pos.inc
#
# On slave:
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/sync_slave_io_with_master.inc
#
# Parameters:
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= sync_io_with_master.inc
--source include/begin_include_file.inc


--let $_old_slave_error_param= $slave_error_param
--let $slave_error_param= Last_IO_Errno

--let $slave_param= Master_Log_File
--let $slave_param_value= $_master_file
source include/wait_for_slave_param.inc;

let $slave_param= Read_Master_Log_Pos;
let $slave_param_value= $_master_pos;
source include/wait_for_slave_param.inc;

--let $slave_error_param= $_old_slave_error_param


--let $include_filename= sync_io_with_master.inc
--source include/end_include_file.inc
