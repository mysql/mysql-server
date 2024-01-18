# include/wait_until_transaction_prepared.inc
#
# SUMMARY
#
#    Waits until XA transactions are in a PREPARED state
#
# USAGE
#
#    [--let $expected_prepared_xa_count= 1] # Wait for $prepared_count transactions
#    [--let $wait_timeout= 30] # Override default 6000 ms with 3000 ms
#    --source include/wait_until_transaction_prepared.inc
#

--let $wait_for_prepared_count= 1

if ($expected_prepared_xa_count)
{
  --let $wait_for_prepared_count= `SELECT $expected_prepared_xa_count`
}

let $wait_counter= 60;
if ($wait_timeout)
{
  --let $wait_counter= `SELECT $wait_timeout`
}

let $success= 0;
--disable_query_log
while ($wait_counter)
{
  --let $statement = XA RECOVER
  --let $column = data
  --source include/get_row_count.inc
  if($row_count == $wait_for_prepared_count)
  {
    --let $success= 1
    --let $retry_counter= 0
    --let $wait_counter= 0
  }
  if(!$success)
  {
    --sleep 0.1
    --dec $wait_counter
  }
}
--enable_query_log
if (!$success)
{
  --source include/rpl/debug/show_debug_info.inc
  --echo "Timeout in wait_until_transaction_prepared.inc"
  --echo "wait_for_prepared_count = $wait_for_prepared_count"
  --echo "wait_timeout = $wait_timeout"
  --die
}
