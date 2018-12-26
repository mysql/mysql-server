# ==== Purpose ====
#
# Reset group_replication_bootstrap_group option on all servers,
# and then set group_replication_bootstrap_group option on current
# server.
#
# --source include/gr_set_bootstrap_group.inc

--let $_include_silent_group_replication_set_bootstrap_group= $include_silent
--let $include_silent= 1
--let $include_filename= gr_set_bootstrap_group.inc
--source include/begin_include_file.inc

# Reset group_replication_bootstrap_group option on all servers.
--source include/gr_clear_bootstrap_group.inc

# Set group_replication_bootstrap_group option.
--disable_query_log
--eval SET GLOBAL group_replication_bootstrap_group= 1
--enable_query_log

--let $include_filename= gr_set_bootstrap_group.inc
--source include/end_include_file.inc

--let $include_silent= $_include_silent_group_replication_set_bootstrap_group
