--let $option = query_get_value(SHOW VARIABLES LIKE "%replica_preserve_commit_order%", Value, 1)
if ($option == 'OFF')
{
  --skip The test is compatible only with replica_preserve_commit_order=ON
}
