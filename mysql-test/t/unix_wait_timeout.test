
--source include/not_windows.inc

--echo # Bug #34007830: Reconnect option is not working

--echo #
--echo # Test UNIX domain sockets timeout with reconnect.
--echo #

--echo # Open con2 and set a timeout.
connect(con2,localhost,root,,);

SET @is_old_connection = 1;
SELECT @is_old_connection;

LET $ID= `SELECT connection_id()`;
SET @@SESSION.wait_timeout = 2;

--echo # Wait for con2 to be disconnected.
connection default;
let $wait_condition=
  SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE ID = $ID;
--source include/wait_condition.inc

--echo # Check that con2 has been reconnected.
connection con2;
connect;
SELECT "Unix domain socket will hit wait_timeout with reconnect, still succeed as reconnect is enabled.";
SELECT @is_old_connection;
connection default;
disconnect con2;
