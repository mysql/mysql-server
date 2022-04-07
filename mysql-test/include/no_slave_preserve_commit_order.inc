--let $option = query_get_value(SHOW VARIABLES LIKE "%slave_preserve_commit_order%", Value, 1)
if ($option == 'ON')
{
  --skip The test is incompatible with slave_preserve_commit_order=ON
}
