# ==== Purpose ====
#
# To verify that an expected crash happened at the $debug_point when executing
# the $statement.
#
# ==== Implementation ====
#
# 1. Add the $debug_point.
# 2. Verify that an expected crash happened at the $debug_point when executing
#    the $statement.
# 3. Start the server.
#
# ==== Usage ====
#
# --let $debug_point=
# --let $statement=
# --source common/binlog/server_crash_recovery.inc
#
# Parameters:
#
# $debug_point
#   Set a debug point to cause an expected crash when executing the $statement.
#
# $statement
#   The statement to be executed to cause an expected crash.
#   
# ==== References ====
#
# Wl#12080 Add support to binary log encryption key rotation and cleanup

--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $debug_point=$debug_point
--source include/add_debug_point.inc
--error CR_SERVER_LOST
--eval $statement

--source include/wait_until_disconnected.inc
--echo # Restart the master server
--exec echo $restart_parameters > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc
