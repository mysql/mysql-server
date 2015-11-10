# Check if ipv4 mapped to ipv6 is available.
--disable_query_log
--disable_abort_on_error
--source include/add_extra_root_users.inc
connect (checkcon123456789,::FFFF:127.0.0.1,root,,test);
if($mysql_errno)
{
--source include/remove_extra_root_users.inc
skip wrong IP;
}
connection default;
disconnect checkcon123456789;
--source include/remove_extra_root_users.inc
--enable_abort_on_error
--enable_query_log
# end check

