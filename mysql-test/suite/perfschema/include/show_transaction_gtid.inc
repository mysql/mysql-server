# ==== Purpose ====
#
# Auxiliary file used by transaction_gtid.test
#
# Invoked to check the contents of the
# performance_schema.events_transaction_[current|history] tables and
# write the result to the result log.  This is executed on the
# 'server_1' connection and shows only status of transactions on the
# 'default' connection.

--connection server_1

--replace_result $server_uuid SERVER_UUID aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa A
--let $history_result= `SELECT GTID FROM performance_schema.events_transactions_history WHERE THREAD_ID = $thread_id`
if ($history_result == '')
{
  --let $history_result= none
}

--replace_result $server_uuid SERVER_UUID aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa A
--let $current_result= `SELECT GTID FROM performance_schema.events_transactions_current WHERE THREAD_ID = $thread_id`
if ($current_result == '')
{
  --let $current_result= none
}

--echo - history=$history_result current=$current_result

--connection default
