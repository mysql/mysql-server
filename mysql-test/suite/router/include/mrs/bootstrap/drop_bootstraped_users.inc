
--disable_query_log
--disable_result_log
--let $_no_of_users=0
--let $_bootstraped_user=`SELECT user from mysql.user WHERE user like "mysql_router1_%" LIMIT 1;`
while ($_bootstraped_user)
{
  eval DROP USER $_bootstraped_user@'%';
  --let $_bootstraped_user=`SELECT user from mysql.user WHERE user like "mysql_router1_%" LIMIT 1;`
  --inc $_no_of_users
}

echo # Removed bootstraped users: $_no_of_users;

--enable_query_log
--enable_result_log
