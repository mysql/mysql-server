# Check if lo0 interface is configured
# and has local scope address assigned
#

--disable_query_log
--disable_result_log
--disable_abort_on_error

--exec ifconfig lo0 | grep -i fe80::1
--let $status=$__error

if ($status != 0)
{
    skip No lo0 interface or fe80::1 (local-scope address) is missing;
}

--enable_abort_on_error
--enable_result_log
--enable_query_log
# end check

