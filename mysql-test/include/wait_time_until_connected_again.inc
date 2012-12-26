#
# Include this script to wait until the connection to the
# server has been restored or timeout occurs
# Unlike wait_until_connected_again the variable counter must be set outside as var. 
# set WAIT_COUNT before executinmg this include file.
--disable_result_log
--disable_query_log
let $counter= $WAIT_COUNT;
let $mysql_errno= 9999;
while ($mysql_errno)
{
  # Strangely enough, the server might return "Too many connections"
  # while being shutdown, thus 1040 is an "allowed" error
  # See BUG#36228
  --error 0,1040,1053,2002,2003,2006,2013
  show status;

  dec $counter;
  if (!$counter)
  {
    --echo Waited $WAIT_COUNT * 0.5 sec.
    --die Server failed to restart
  }
  --sleep 0.5
}
--enable_query_log
--enable_result_log
