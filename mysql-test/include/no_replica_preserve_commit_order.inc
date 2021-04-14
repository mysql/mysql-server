--let $option = query_get_value(SHOW VARIABLES LIKE "%replica_preserve_commit_order%", Value, 1)
if ($option == 'ON')
{
  --skip The test is incompatible with replica_preserve_commit_order=ON
}
