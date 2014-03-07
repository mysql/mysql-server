# ==== Purpose ====
#
# Wait until rpl_semi_sync_master_status equals to ON
#
# ==== Usage ====
#
# --source include/wait_for_semisync_master_status_on.inc
#
--let $status_var= rpl_semi_sync_master_status
--let $status_var_value= ON
--source include/wait_for_status_var.inc
