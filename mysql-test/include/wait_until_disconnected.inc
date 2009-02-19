# Include this script after a shutdown to wait until the connection
# to the server has been lost or timeout occurs.
# When you change this file you may have to chance its cousin
# wait_until_connected_again.inc

--disable_result_log
--disable_query_log
let $counter= 600;
let $mysql_errno= 0;
while (!$mysql_errno)
{
  # Strangely enough, the server might return "Too many connections"
  # while being shutdown, thus 1040 is an "allowed" error.
  # See BUG#36228.
  --error 0,1040,1053,2002,2003,2006,2013
  show status;

  dec $counter;
  if (!$counter)
  {
    --die Server failed to dissapear
  }
  --real_sleep 0.1
}
--enable_query_log
--enable_result_log
