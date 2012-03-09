# Check if ipv6 is available.
#
--disable_query_log
--disable_result_log
--disable_abort_on_error
connect (checkcon123456789,::1,root,,test);
if($mysql_errno)
{
    skip No IPv6 support;
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

