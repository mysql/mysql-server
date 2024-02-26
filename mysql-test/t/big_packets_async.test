#
# Test sending and receiving queries that must span packets(ie,larger than 16mb)
#

# Save the initial number of concurrent sessions
--source include/count_sessions.inc
# Requires big_test option
--source include/big_test.inc

--let $CLIENT_TYPE = NONBLOCKING
--source big_packets.inc


# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
