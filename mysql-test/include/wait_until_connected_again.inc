#
# Include this script to wait until the connection to the
# server has been restored or timeout occurs.
# You should have done --enable_reconnect first
--disable_result_log
--disable_query_log
let $counter= 5000;
let $mysql_errno= 1;
while ($mysql_errno)
{
  --error 0,2002,2003,2006,1053
  show status;

  dec $counter;
  if (!$counter)
  {
    --die Server failed to restart
  }
  --sleep 0.1
}
--enable_query_log
--enable_result_log
