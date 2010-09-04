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
# ==== Usage ====
#
# --source extra/rpl_tests/delayed_slave_wait_on_query.inc

connection master;
--echo [on slave]
--let $slave_timeout= $time1

--source include/sync_slave_io_with_master.inc
--echo # sleep 1*T
--sleep $time1

--echo # Expect query not executed and status is 'Waiting until MASTER_DELAY...'
SELECT * FROM t1 ORDER BY b DESC LIMIT 1;
--source include/show_delayed_slave_state.inc

--echo # sleep 1*T
--sleep $time1

--echo # sync with master (with timeout 1*T)
--source include/sync_with_master.inc

--echo # Expect query executed and status is 'Has read all relay log...'
SELECT * FROM t1 ORDER BY b DESC LIMIT 1;
--source include/show_delayed_slave_state.inc

--source include/check_slave_is_running.inc
