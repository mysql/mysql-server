#
# SUMMARY
#    Ensure all the connections are disconnected including default connection.
#
# USAGE
#     This is needed to complete a plugin uninstall
#
# EXAMPLE
#    audit_log_bugs.test, query_cache.test
#
--source include/count_sessions.inc
disconnect default;
connect(default, localhost, root,,);
--source include/wait_until_count_sessions.inc
