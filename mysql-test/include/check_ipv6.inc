# Check if ipv6 is available.
#
# Parameters:
#  $check_ipv6_just_check - don't skip the test if IPv6 is unsupported,
#                           just set the variable $check_ipv6_supported
#
--disable_query_log
--disable_result_log
--disable_abort_on_error
let $check_ipv6_supported=1;
connect (checkcon123456789,::1,root,,test);
if($mysql_errno)
{
  let $check_ipv6_supported=0;
  if(!$check_ipv6_just_check)
  {
    skip No IPv6 support;
  }
}
connection default;
if(!$mysql_errno)
{
  disconnect checkcon123456789;
}
--enable_abort_on_error
--enable_result_log
--enable_query_log
# end check

