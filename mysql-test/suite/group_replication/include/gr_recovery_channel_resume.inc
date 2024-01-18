################################################################################
# See gr_recovery_channel_block.inc
################################################################################

--let $include_filename= gr_recovery_channel_resume.inc
--source include/begin_include_file.inc

--let $_saved_connection= $CURRENT_CONNECTION
--disable_query_log

--let $rpl_connection_name= server_2
--source include/connection.inc
UNLOCK TABLES;

--let $group_replication_member_state= ONLINE
--source include/gr_wait_for_member_state.inc

--enable_query_log
--connection $_saved_connection

--let $include_filename= gr_recovery_channel_resume.inc
--source include/end_include_file.inc
