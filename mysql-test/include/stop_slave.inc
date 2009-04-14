# ==== Purpose ====
#
# Issues STOP SLAVE on the current connection. Then waits until both
# the IO and SQL threads have stopped, or until a timeout is reached.
#
# Please use this instead of 'STOP SLAVE', to reduce the risk of test
# case bugs.
#
# ==== Usage ====
#
# source include/wait_for_slave_to_start.inc;
#
# Parameters to this macro are $slave_timeout and
# $master_connection. See wait_for_slave_param.inc for
# descriptions.

--disable_query_log
STOP SLAVE;
--enable_query_log
--echo include/stop_slave.inc
source include/wait_for_slave_to_stop.inc;
