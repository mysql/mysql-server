--disable_query_log
--disable_result_log
--let $submode= query_get_value(SHOW VARIABLES LIKE "%slave_parallel_type%", Value, 1)
if ($submode != 'LOGICAL_CLOCK')
{
  --skip The test needs slave_parallel_type=logical_clock
}
--enable_result_log
--enable_query_log

