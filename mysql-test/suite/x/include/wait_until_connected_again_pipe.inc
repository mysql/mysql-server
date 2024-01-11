#
# Include this script to wait until the connection to the
# server has been restored or timeout occurs
--disable_result_log
--disable_query_log
let $counter= 500;
if ($VALGRIND_TEST) {
  let $counter= 3000;
}
let $mysql_errno= 9999;
while ($mysql_errno)
{

  # Strangely enough, the server might return "Too many connections"
  # while being shutdown, thus 1040 is an "allowed" error
  # See BUG#36228
  --error 0,ER_CON_COUNT_ERROR,ER_SERVER_SHUTDOWN,2002,2003,2006,2013,ER_ACCESS_DENIED_ERROR,ER_SECURE_TRANSPORT_REQUIRED,2016,2017,ER_CLIENT_INTERACTION_TIMEOUT
  connect;
  if ($mysql_errno == 1045){
    let mysql_errno=0;
  }
  if ($mysql_errname == ER_SECURE_TRANSPORT_REQUIRED){
    let mysql_errno=0;
  }
  dec $counter;
  if (!$counter)
  {
    --die Server failed to restart
  }
  --sleep 0.1
}
--enable_query_log
--enable_result_log
