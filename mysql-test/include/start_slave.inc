# ==== Purpose ====
#
# Issues START SLAVE on the current connection. Then waits until both
# the IO and SQL threads have started, or until a timeout is reached.
#
# Please use this instead of 'START SLAVE', to reduce the risk of test
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
START SLAVE;
--enable_query_log
--echo include/start_slave.inc
source include/wait_for_slave_to_start.inc;
