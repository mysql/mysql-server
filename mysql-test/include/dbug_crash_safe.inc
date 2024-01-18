# ==== Purpose ====
#
# Make the program fail, without creating a core file.
#
# This should be used only for debugging test cases, when you need the server to
# fail, without creating a core file. To stop a server as part of a test
# scenario, use one of rpl/stop_server.inc, rpl/restart_server.inc,
# kill_mysqld.inc, restart_mysqld.inc, etc.
#
# ==== Implementation ====
#
# This relies on a debug symbol that causes the server to call
# DBUG_SUICIDE(). So it only works on debug-compiled servers.
#
# ==== Usage ====
#
# [in main test: source include/have_dbug.inc]
# --source include/dbug_crash_safe.inc

--source include/expect_crash.inc
--error CR_SERVER_LOST
SET SESSION debug = '+d,crash_now_safe';
