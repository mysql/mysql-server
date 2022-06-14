#
# TC1. `START SLAVE` issues an info message that includes applier user
#      info
# --------------------------------------------------------------------
#
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/wait_for_slave_to_start.inc
include/assert_grep.inc [Start slave message found in log contains applier user info]
#
# TC2. Start server with a channel configured issues message with applier
#      user info
# -----------------------------------------------------------------------
#
include/rpl_restart_server.inc [server_number=2 parameters: --skip_replica_start=FALSE]
include/wait_for_slave_to_start.inc
include/assert_grep.inc [Start slave message with user info found twice in log, after restart]
include/rpl_restart_server.inc [server_number=2]
include/start_slave.inc
include/rpl_end.inc
