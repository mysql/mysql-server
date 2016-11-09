# ==== Purpose ====
#
# Starts and bootstraps a group on the current server.
#
# The following steps are done:
#   1) set group_replication_bootstrap_group option on current
#      server;
#   2) execute START GROUP_REPLICATION on the current server and
#      wait until member state is ONLINE;
#   3) reset group_replication_bootstrap_group option on all
#      servers.
#
# ==== Usage ====
#
# --source ../inc/start_and_bootstrap_group_replication.inc
#

--let $include_filename= start_and_bootstrap_group_replication.inc
--source include/begin_include_file.inc

--disable_query_log
SET GLOBAL group_replication_bootstrap_group= 1;
--enable_query_log
--source include/start_group_replication.inc
--source ../inc/gr_clear_bootstrap_group.inc

--let $include_filename= start_and_bootstrap_group_replication.inc
--source include/end_include_file.inc
