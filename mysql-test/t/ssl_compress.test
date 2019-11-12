#Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc
--source include/allowed_ciphers.inc
# Turn on compression between the client and server
# and run a number of tests


# Save the initial number of concurrent sessions
--source include/count_sessions.inc

connect (ssl_compress_con,localhost,root,,,,,SSL COMPRESS);

# Check ssl turned on
--replace_regex $ALLOWED_CIPHERS_REGEX
SHOW STATUS LIKE 'Ssl_cipher';

# Check compression turned on
SHOW STATUS LIKE 'Compression';

# Source select test case
-- source include/common-tests.inc

# Check ssl turned on
--replace_regex $ALLOWED_CIPHERS_REGEX
SHOW STATUS LIKE 'Ssl_cipher';

# Check compression turned on
SHOW STATUS LIKE 'Compression';

connection default;
disconnect ssl_compress_con;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

##  This test file is for testing encrypted communication only, not other
##  encryption routines that the SSL library happens to provide!
