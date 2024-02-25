# Bug #34007830: Reconnect option is not working
#
# Test UNIX domain sockets timeout with reconnect.
#
# Open con2 and set a timeout.
--enable_reconnect;
SET @is_old_connection = 1;
SELECT @is_old_connection;
@is_old_connection
1
SET @@SESSION.wait_timeout = 2;
# Wait for con2 to be disconnected.
# Check that con2 has been reconnected.
SELECT "Unix domain socket will hit wait_timeout with reconnect, still succeed as reconnect is enabled.";
Unix domain socket will hit wait_timeout with reconnect, still succeed as reconnect is enabled.
Unix domain socket will hit wait_timeout with reconnect, still succeed as reconnect is enabled.
SELECT @is_old_connection;
@is_old_connection
NULL
