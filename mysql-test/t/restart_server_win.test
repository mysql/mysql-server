--source include/windows.inc

# Check RESTART of standalone server under windows.

RESTART;
--source include/wait_until_disconnected.inc
# Wait until server comes up.
--source include/wait_until_connected_again.inc
--echo # Executing a sql command after RESTART.
SELECT 1;

--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
# Shutdown and restart mysqld of mtr.
SHUTDOWN;
# Wait for mysqld to come up.
--source include/wait_until_disconnected.inc
--source include/wait_until_connected_again.inc
