#
## Create several MySQL Server account that use different authentication
## plugins.
#
# [--let $user_native=user_name]
# [--let $user_sha256=user_name]
# [--let $user_cache2=user_name]
#
# [--let $host_native=host_name_or_ip]
# [--let $host_sha256=host_name_or_ip]
# [--let $host_cache2=host_name_or_ip]
#
# [--let $pass_native=password]
# [--let $pass_sha256=password]
# [--let $pass_cache2=password]
# [--let $all_users_have_the_same_name=overwrite_name_for_all_accounts]
#
# --source ../include/create_mysql_accounts_for_xplugin_tests.h
#

#
## Defaults for user names
#

if ($user_native == "")
{
  --let $user_native=xuser_native
}

if ($user_sha256 == "")
{
  --let $user_sha256=xuser_sha256
}

if ($user_cache2 == "")
{
  --let $user_cache2=xuser_cache2
}


#
## Defaults for user hosts
#

if ($host_native == "")
{
  --let $host_native='localhost'
}

if ($host_sha256 == "")
{
  --let $host_sha256='localhost'
}

if ($host_cache2 == "")
{
  --let $host_cache2='localhost'
}


#
## Defaults for user passwords
#

if ($pass_native == "")
{
  --let $pass_native='native'
}

if ($pass_sha256 == "")
{
  --let $pass_sha256='sha256'
}

if ($pass_cache2 == "")
{
  --let $pass_cache2='cache2'
}

if ($all_users_have_the_same_name != "")
{
  --let $user_native=$all_users_have_the_same_name
  --let $user_sha256=$all_users_have_the_same_name
  --let $user_cache2=$all_users_have_the_same_name
}

eval CREATE USER $user_native@$host_native
            IDENTIFIED WITH 'mysql_native_password' BY $pass_native;

eval CREATE USER $user_sha256@$host_sha256
            IDENTIFIED WITH 'sha256_password' BY $pass_sha256;

eval CREATE USER $user_cache2@$host_cache2
            IDENTIFIED WITH 'caching_sha2_password' BY $pass_cache2;

