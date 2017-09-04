# Auxaliary file for run binlog_unsafe_commit_parent.test
# Usage:
# --let $admin_statement= REPAIR TABLE t1
# --source extra/binlog_tests/binlog_unsafe_commit_parent.inc
#
# It uses two debug sync points to guarantee that:
# - Both admin statement of admin_conn connection and the INSERT statement
#   of insert_conn connection go into the same flush queue.
# - The admin statement becomes the head of the flush queue.
# - The INSERT statement follows admin statement.

--let $rpl_connection_name= admin_conn
--source include/rpl_connection.inc
SET debug_sync = "waiting_in_the_middle_of_flush_stage SIGNAL admin_ready
                  WAIT_FOR admin_continue";
send_eval $admin_statement;

--let $rpl_connection_name= insert_conn
--source include/rpl_connection.inc
SET debug_sync = "now WAIT_FOR admin_ready";
SET debug_sync = "bgc_after_enrolling_for_flush_stage SIGNAL insert_ready
                  WAIT_FOR insert_continue";
send_eval INSERT INTO t1(c2) VALUES("$admin_statement");

--let $rpl_connection_name= default
--source include/rpl_connection.inc
SET debug_sync = "now WAIT_FOR insert_ready";
SET debug_sync = "now SIGNAL admin_continue,insert_continue";

--let $rpl_connection_name= admin_conn
--source include/rpl_connection.inc
--reap

--let $rpl_connection_name= insert_conn
--source include/rpl_connection.inc
--reap
SET debug_sync = "RESET";


