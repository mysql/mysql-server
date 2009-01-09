# ==== Purpose ====
#
# Waits until the slave IO thread has been synced, i.e., all events
# have been copied over to slave.  Does not care if the SQL thread is
# in sync.
#
#
# ==== Usage ====
#
# source include/sync_slave_io_with_master.inc;
#
# Syncs to the current position on master, as found by SHOW MASTER
# STATUS.
#
# Must be called on the master. Will change connection to the slave.
#
# Parameters to this macro are $slave_timeout and
# $master_connection. See wait_for_slave_param.inc for
# descriptions.

let $_master_file= query_get_value("SHOW MASTER STATUS", File, 1);
let $_master_pos= query_get_value("SHOW MASTER STATUS", Position, 1);

connection slave;

let $slave_error_message= Failed while waiting for slave IO thread to sync;

let $slave_param= Master_Log_File;
let $slave_param_value= $_master_file;
source include/wait_for_slave_param.inc;

let $slave_param= Read_Master_Log_Pos;
let $slave_param_value= $_master_pos;
source include/wait_for_slave_param.inc;

let $slave_error_message= ;
