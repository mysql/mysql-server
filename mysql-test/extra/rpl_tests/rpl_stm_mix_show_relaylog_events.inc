# BUG#28777 SHOW BINLOG EVENTS does not work on relay log files
#
# GOAL
# ==== 
# 
#   Test that SHOW BINLOG EVENTS and the new SHOW RELAYLOG EVENTS works after
#   the patch, both on master and slave.
#
# HOW
# ===
#
#   This test issues SHOW [BINLOG|RELAYLOG] EVENTS both on master and slave after 
#   some statements have been issued.

-- source include/master-slave.inc
-- source include/have_binlog_format_mixed_or_statement.inc
-- source include/not_mts_slave_parallel_workers.inc

--let $mask_binlog_commit_events= 1
-- source extra/rpl_tests/rpl_show_relaylog_events.inc
--let $mask_binlog_commit_events= 0
--source include/rpl_end.inc
