#
# Include this script to wait until the connection to the
# server has been restored or timeout occurs
#
# $wait_counter - can be set before calling the script, will be reset at the end
#
# $explicit_default_counter - can be set before calling the script,
#                             reset only at the end of the test, overrides $wait_counter


--disable_result_log
--disable_query_log

--let $counter= 1000
if ($explicit_default_counter)
{
  --let $wait_counter= $explicit_default_counter
}
if ($VALGRIND_TEST) {
  --let $wait_counter= 9000
}
if ($TSAN_OPTIONS) {
  --let $wait_counter= 15000
}
if ($wait_counter) {
  --let $counter= $wait_counter
}
--let $mysql_errno= 9999
while ($mysql_errno)
{

  # Strangely enough, the server might return "Too many connections"
  # while being shutdown, thus 1040 is an "allowed" error
  # See BUG#36228
  --error 0,1040,1053,2002,2003,2006,2013,1045,ER_SECURE_TRANSPORT_REQUIRED,2016,2017
  show session status;
  if ($mysql_errno == 1045){
    --let mysql_errno=0
  }
  if ($mysql_errname == ER_SECURE_TRANSPORT_REQUIRED){
    --let mysql_errno=0
  }
  --dec $counter
  if (!$counter)
  {
    --die Server failed to restart
  }
  --sleep 0.1
}
# Reset $wait_counter so that its value won't be used on subsequent
# calls, and default will be used instead.
if (!$explicit_default_counter)
{
  --let $wait_counter= 0
}
--enable_query_log
--enable_result_log
