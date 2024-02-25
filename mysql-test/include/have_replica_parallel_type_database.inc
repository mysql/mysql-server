--disable_query_log
--disable_result_log
--let $submode= query_get_value(SHOW VARIABLES LIKE "%replica_parallel_type%", Value, 1)
if ($submode != 'DATABASE')
{
  --skip The test needs replica_parallel_type=database
}
--enable_result_log
--enable_query_log

