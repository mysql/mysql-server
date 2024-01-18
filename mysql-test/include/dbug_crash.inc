# ==== Purpose ====
#
# Crash the server of the current connection.
#
# This should be used only for debugging test cases, when you need the
# core file. To stop a server as part of a test scenario, use one of
# rpl/stop_server.inc, rpl/restart_server.inc, kill_mysqld.inc,
# restart_mysqld.inc, etc.
#
# ==== Implementation ====
#
# This relies on a debug symbol that causes the server to call
# DBUG_ASSERT(0).  So it only works on debug-compiled servers.
#
# ==== Usage ====
#
# [in main test: source include/have_dbug.inc]
# --source include/dbug_crash.inc

SET SESSION debug = 'd,crash_now';
