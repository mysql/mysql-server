--disable_query_log
--disable_result_log
--let $scan_count=10

# Run some (round robin) scans to ensure normal number
# of seized api connect records
while ($scan_count)
{
  select * from test.t1;
  --dec $scan_count
}

# Count TC ApiConnectRecords not in states
#   1 (CS_DISCONNCTED)
#   7 (CS_RESTART)
select count(1) into @acc_count from ndbinfo.ndb$transactions_full where state != 7 and state != 1;

let $api_conn_count=query_get_value(select @acc_count as acc, acc, 1);
--enable_result_log
--enable_query_log
