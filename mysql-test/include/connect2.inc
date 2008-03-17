# include/connect2.inc
#
# SUMMARY
# 
#   Make several attempts to connect.
#
# USAGE
#
# EXAMPLE
# 
#   connect.test
#

--disable_query_log

let $wait_counter= 300;
if ($wait_timeout)
{
  let $wait_counter= `SELECT $wait_timeout * 10`;
}
# Reset $wait_timeout so that its value won't be used on subsequent
# calls, and default will be used instead.
let $wait_timeout= 0;

--echo # -- Establishing connection '$con_name' (user: $con_user_name)...

while ($wait_counter)
{
    --disable_abort_on_error
    --disable_result_log
    --connect ($con_name,localhost,$con_user_name)
    --enable_result_log
    --enable_abort_on_error

    let $error = $mysql_errno;

    if (!$error)
    {
        let $wait_counter= 0;
    }
    if ($error)
    {
        real_sleep 0.1;
        dec $wait_counter;
    }
}
if ($error)
{
  --echo # -- Error: can not establish connection '$con_name'.
}
if (!$error)
{
  --echo # -- Connection '$con_name' has been established.
}

--enable_query_log
