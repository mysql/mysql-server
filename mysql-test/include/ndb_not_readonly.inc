# Check that server has come out ot readonly mode
#
# wait for server to connect properly to cluster
#

--disable_query_log

set @saved_log = @@sql_log_bin;
set sql_log_bin = 0;
--error 0,ER_NO_SUCH_TABLE,ER_OPEN_AS_READONLY,ER_GET_ERRMSG,ER_KEY_NOT_FOUND
delete from mysql.ndb_apply_status where server_id=0;
let $mysql_errno= 1;
let $counter= 600;
while ($mysql_errno)
{
  # Table is readonly until the mysqld has connected properly
  --error 0,ER_NO_SUCH_TABLE,ER_OPEN_AS_READONLY,ER_GET_ERRMSG
  replace into mysql.ndb_apply_status values(0,0,"",0,0);
  if ($mysql_errno)
  {
    if (!$counter)
    {
      die Failed while waiting for mysqld to come out of readonly mode;
    }
    dec $counter;
    --sleep 0.1
  }
}
delete from mysql.ndb_apply_status where server_id=0;
set sql_log_bin = @saved_log;

--enable_query_log

#
# connected
#
