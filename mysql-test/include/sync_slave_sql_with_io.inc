# ==== Purpose ====
#
# Sync the slave SQL thread with the IO thread on the current connection.
#
# ==== Usage ====
#
# [--let $use_gtids= 1]
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/sync_slave_sql_with_io.inc
#
# Parameters:
#
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.
#
#   $slave_timeout
#     By default, the synchronization timeouts after 300 seconds. If
#     $slave_timeout is set, the synchronization timeouts after
#     $slave_timeout seconds.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= sync_slave_sql_with_io.inc
--source include/begin_include_file.inc

--source include/save_io_thread_pos.inc
--source include/sync_slave_sql.inc

--let $include_filename= sync_slave_sql_with_io.inc
--source include/end_include_file.inc
