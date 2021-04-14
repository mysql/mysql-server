# ==== Purpose ====
#
# Auxiliary file used by rpl_delayed_slave.test.  This assumes that an
# 'INSERT INTO t1...' query has been executed on the master.  It does
# this:
#
# - After half the delay, check the status. It should be delaying and
#   the query should not have executed.
#
# - After one and a half delay, check the status. It should not be
#   delaying and the query should be executed.
#
#
# ==== Usage ====
#
# --let $query_number= 4
# --source extra/rpl_tests/delayed_slave_wait_on_query.inc
#
# Parameters:
# $query_number
#   The value of the 'b' column in t1 for the row inserted by the query
#   we are waiting for.

connection master;

--echo [on slave]
--let $slave_timeout= $time1
--source include/sync_slave_io_with_master.inc
--echo # sleep 1*T
--sleep $time1

--let $assert_text= Query $query_number should not be executed
--let $assert_cond= MAX(b) < $query_number FROM t1
--source include/assert.inc

--let $assert_text= Status should be 'Waiting until SOURCE_DELAY...'
--let $assert_cond= "[SHOW SLAVE STATUS, Slave_SQL_Running_State, 1]" LIKE "Waiting until SOURCE_DELAY%"
--source include/assert.inc

--echo # sleep 1*T
--sleep $time1

--echo # sync with master (with timeout 1*T)
--source include/sync_slave_sql.inc

--let $assert_text= Query $query_number should be executed
--let $assert_cond= MAX(b) = $query_number FROM t1
--source include/assert.inc

--let $assert_text= Status should be 'Has read all relay log...'
--let $assert_cond= "[SHOW SLAVE STATUS, Slave_SQL_Running_State, 1]" LIKE "Replica has read all relay log%"
--source include/assert.inc


--source include/check_slave_is_running.inc
