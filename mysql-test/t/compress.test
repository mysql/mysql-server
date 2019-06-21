#Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc
# Turn on compression between the client and server
# and run a number of tests

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

connect (comp_con,localhost,root,,,,,COMPRESS);
# Check compression turned on
SHOW STATUS LIKE 'Compression%';

# Source select test case
-- source include/common-tests.inc

connection default;
disconnect comp_con;
# Test zstd compressed connection.
connect(zstd_con, localhost, root,,,,,,,zstd,10);

# Check compression turned on
SHOW STATUS LIKE 'Compression%';

--source include/common-tests.inc
connection default;
disconnect zstd_con;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
