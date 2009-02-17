#
# Include this script to wait until the connection to the
# server has been dropped
--disable_result_log
--disable_query_log
let $counter= 500;
let $mysql_errno= 0;
while (!$mysql_errno)
{
  --error 0,1053,2002,2006,2013
  show status;

  dec $counter;
  if (!$counter)
  {
    --die Server failed to dissapear
  }
  --sleep 0.1
}
--enable_query_log
--enable_result_log
