#
# Test sending and receiving queries that saddle a MAX_PACKET_LENGTH
# boundary (aka 16mb boundary)
#

# Save the initial number of concurrent sessions
--source include/count_sessions.inc
# Requires big_test option
--source include/big_test.inc

--let CLIENT_TYPE = BLOCKING
--source big_packets_boundary.inc

# cleanup
# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
