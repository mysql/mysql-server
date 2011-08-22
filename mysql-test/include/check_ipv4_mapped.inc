# Check if ipv4 mapped to ipv6 is available.
#
# Parameters:
#  $check_ipv4_mapped_just_check - don't skip the test if IPv4 mapped is unsupported,
#                                  just set the variable $check_ipv4_mapped_supported
#
--disable_query_log
--disable_result_log
--disable_abort_on_error
let $check_ipv4_mapped_supported=1;
connect (checkcon123456789a,::FFFF:127.0.0.1,root,,test);
if($mysql_errno)
{
  let $check_ipv4_mapped_supported=0;
  if(!$check_ipv4_mapped_just_check)
  {
    skip No mapped IPv4 support;
  }
}
connection default;
if(!$mysql_errno)
{
  disconnect checkcon123456789a;
}
--enable_abort_on_error
--enable_result_log
--enable_query_log
# end check

