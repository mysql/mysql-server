# ==== Purpose ====
#
# Save the master file and position of the current connection in
# internal variables. This can later be used together with
# sync_with_master.inc
#
# ==== Usage ====
#
# --source include/save_master_pos.inc
#
# This will save the binlog position of the current connection in the
# mysqltest variables $_saved_master_file and $_saved_master_pos.

let $_saved_master_file= query_get_value("SHOW MASTER STATUS", File, 1);
let $_saved_master_pos= query_get_value("SHOW MASTER STATUS", Position, 1);
