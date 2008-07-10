# Check that mysqld has reconnected to ndbd after
# restart of ndbd
#
--disable_query_log
--disable_result_log
let $mysql_errno= 1;
let $counter= 600;
while ($mysql_errno)
{
  --error 0,157
  CREATE TABLE ndb_wait_connected (a int primary key);
  if ($mysql_errno)
  {
    if (!$counter)
    {
      die Failed waiting for mysqld to reconnect to ndbd;
    }
    dec $counter;
    --sleep 0.1
  }
}
DROP TABLE ndb_wait_connected;
--enable_query_log
--enable_result_log


