# ==== Purpose ====
#
# Save the current binlog position on the master, just like the
# built-in mysqltest command save_master_pos.  The advantage of this
# script is that the saved position is available to the test script.
#
#
# ==== Usage ====
#
# [--let $rpl_debug= 1]
# --source include/save_master_pos.inc
#
# Typically, you would use this script together with
# include/sync_io_with_master.inc
#
# Parameters:
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= save_master_pos.inc
--source include/begin_include_file.inc

let $_master_file= query_get_value("SHOW MASTER STATUS", File, 1);
let $_master_pos= query_get_value("SHOW MASTER STATUS", Position, 1);

if ($rpl_debug)
{
  --echo save_master_pos saved file='$_master_file', pos='$_master_pos'
}

--let $include_filename= save_master_pos.inc
--source include/end_include_file.inc
