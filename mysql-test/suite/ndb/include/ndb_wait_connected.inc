# Check that mysqld has reconnected to ndbd
#
--disable_query_log
--disable_result_log
let $mysql_errno= 1;
let $counter= 100;
while ($mysql_errno)
{
  call mtr.add_suppression("Failed to acquire global schema lock");
  --error 0,157,1296
  CREATE TABLE ndb_wait_connected (a int primary key) engine ndb;
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

