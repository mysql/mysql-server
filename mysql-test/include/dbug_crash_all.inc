# ==== Purpose ====
#
# Crash all servers that were configured with rpl_init.inc/master-slave.inc.
#
# This should be used only for debugging test cases, when you need the
# core file. To stop a server as part of a test scenario, use one of
# rpl_stop_server.inc, rpl_restart_server.inc, kill_mysqld.inc,
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
# [--let $connection_suffix = _1]
# --source include/dbug_crash_all.inc
#
# Parameters:
#   $connection_suffix
#     By default, uses connections server_1, server_2, etc.
#     Set this variable to use server_1$connection_suffix,
#     server_2$connection_suffix, etc.


# Tell all servers to crash
--let $_rpl_server = $rpl_server_count
while ($_rpl_server) {
  --connection server_$_rpl_server$server_suffix
  --echo # Crashing server_$_rpl_server$server_suffix
  send SET SESSION debug = 'd,crash_now';
  --dec $_rpl_server
}

# Wait until connection drops
--let $_rpl_server = $rpl_server_count
while ($_rpl_server) {
  --connection server_$_rpl_server$server_suffix
  --error 0,1040,1053,2002,2003,2006,2013,2016,2017
  --reap
  --dec $_rpl_server
}
