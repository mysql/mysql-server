#
## Create several MySQL Server account that use different authentication
## plugins.
#
# [--let $user_cache2=user_name]
# [--let $host_cache2=host_name_or_ip]
# [--let $pass_cache2=password]
# [--let $all_users_have_the_same_name=overwrite_name_for_all_accounts]
#
# --source ../include/create_mysql_accounts_for_xplugin_tests.h
#

#
## Defaults for user names
#

if ($user_cache2 == "")
{
  --let $user_cache2=xuser_cache2
}


#
## Defaults for user hosts
#

if ($host_cache2 == "")
{
  --let $host_cache2='localhost'
}


#
## Defaults for user passwords
#

if ($pass_cache2 == "")
{
  --let $pass_cache2='cache2'
}

if ($all_users_have_the_same_name != "")
{
  --let $user_cache2=$all_users_have_the_same_name
}

eval CREATE USER $user_cache2@$host_cache2
            IDENTIFIED WITH 'caching_sha2_password' BY $pass_cache2;

