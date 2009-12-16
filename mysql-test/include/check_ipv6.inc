# Check if ipv6 is available. If not, server is crashing (see BUG#48915).
--disable_query_log
--disable_abort_on_error
connect (checkcon123456789,::1,root,,test);
if($mysql_errno)
{
skip wrong IP;
}
connection default;
disconnect checkcon123456789;
--enable_abort_on_error
--enable_query_log
# end check

