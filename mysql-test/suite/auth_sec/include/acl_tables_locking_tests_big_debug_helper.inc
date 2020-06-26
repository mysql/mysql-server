#
# Usage:
# --let $ATTEMPT_SQL=<SQL to be attemplted>
# --let $ATTEMPT_DEBUG=<DEBUG value to be set>
# --let $ATTEMPT_DEBUG_SYNC=<DEBUG SYNC value to be set>
# --let $ATTEMPT_EXPECTED_ERROR_NUM=error_number
# --let $EXECUTE_SQL=<SQL to be executed>
# --let $EXECUTE_DEBUG=<DEBUG value to be set>
# --let $EXECUTE_DEBUG_SYNC_1=<DEBUG SYNC value to be set>
# --let $EXECUTE_DEBUG_SYNC_2=<DEBUG SYNC value to be set>
# --let $EXECUTE_EXPECT_ERROR= 0 | 1
# --let $EXECUTE_EXPECTED_ERROR_NUM=error_number
# --source acl_tables_locking_tests_big_debug_helper.inc
#

--enable_connect_log

--connect(conn_attempt, localhost, root,,,,)
SET @@lock_wait_timeout=1;
--connect(conn_execute, localhost, root,,,,)

--connection conn_attempt
if ($ATTEMPT_DEBUG)
{
  --eval SET DEBUG="+d, $ATTEMPT_DEBUG"
}
--eval SET DEBUG_SYNC="$ATTEMPT_DEBUG_SYNC"
--send_eval $ATTEMPT_SQL

--connection conn_execute
--eval SET DEBUG="+d, $EXECUTE_DEBUG"
--eval SET DEBUG_SYNC="$EXECUTE_DEBUG_SYNC_1"
--eval SET DEBUG_SYNC="$EXECUTE_DEBUG_SYNC_2"
if ($EXECUTE_EXPECT_ERROR)
{
  --error $EXECUTE_EXPECTED_ERROR_NUM
}
--eval $EXECUTE_SQL
--eval SET DEBUG="-d, $EXECUTE_DEBUG"

--connection conn_attempt
--error $ATTEMPT_EXPECTED_ERROR_NUM
--reap
if ($ATTEMPT_DEBUG)
{
  --eval SET DEBUG="-d, $ATTEMPT_DEBUG"
}

--connection default
SET DEBUG_SYNC="RESET";
--disconnect conn_attempt
--disconnect conn_execute

--disable_connect_log
