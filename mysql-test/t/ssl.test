#Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc
--source include/allowed_ciphers.inc

# Turn on ssl between the client and server
# and run a number of tests


# Save the initial number of concurrent sessions
--source include/count_sessions.inc

connect (ssl_con,localhost,root,,,,,SSL);

# Check ssl turned on
--replace_regex $ALLOWED_CIPHERS_REGEX
SHOW STATUS LIKE 'Ssl_cipher';

# Check ssl expiration
SHOW STATUS LIKE 'Ssl_server_not_before';
SHOW STATUS LIKE 'Ssl_server_not_after';

# Source select test case
-- source include/common-tests.inc

# Check ssl turned on
--replace_regex $ALLOWED_CIPHERS_REGEX
SHOW STATUS LIKE 'Ssl_cipher';

connection default;
disconnect ssl_con;

--echo #
--echo # Bug#54790: Use of non-blocking mode for sockets limits performance
--echo #

--echo # Open ssl_con and set a timeout.
connect (ssl_con,localhost,root,,,,,SSL);

LET $ID= `SELECT connection_id()`;
SET @@SESSION.wait_timeout = 2;

--echo # Wait for ssl_con to be disconnected.
connection default;
let $wait_condition=
  SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE ID = $ID;
--source include/wait_condition.inc

--echo # Check that ssl_con has been disconnected.
connection ssl_con;
--echo # Different behavior depending on how the plattform implements the 
--error ER_CLIENT_INTERACTION_TIMEOUT,CR_SERVER_LOST
SELECT 1;

connection default;
disconnect ssl_con;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

##  This test file is for testing encrypted communication only, not other
##  encryption routines that the SSL library happens to provide!
