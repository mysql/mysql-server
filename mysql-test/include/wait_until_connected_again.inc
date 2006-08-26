#
# Include this script to wait until the connection to the
# server has been restored or timeout occurs
--disable_result_log
--disable_query_log
let $counter= 100;
while ($mysql_errno)
{
  --error 0,2002,2006
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
