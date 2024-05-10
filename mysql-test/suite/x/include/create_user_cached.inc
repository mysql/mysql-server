## Script create_user_cached.inc - that helps creating mysql user
#         and fills the cache using a secure connection.
#         After this operation the account may be used on
#         unsecure connection.
#
# $user_auth_name -
#    user name.
#
# $user_auth_host -
#    optional value, host name.
#
# $user_auth_plugin -
#    optional value, if not specified then 'caching_sha2_password'
#    will be used.
#
# $user_auth_pass -
#    optional value. users password.
#
## Usage
#
# --let $user_auth_name = someone
# --let $user_auth_host =host
# --let $user_auth_plugin ='caching_sha2_password'
# --let $user_auth_pass = 'secret'
# --source ../include/create_user_cached.inc
#

if (!$user_auth_host)
{
  --let $user_auth_host='localhost'
}

if (!$user_auth_plugin)
{
  --let $user_auth_plugin='caching_sha2_password'
}

if (!$user_auth_pass)
{
  --let $user_auth_pass=""
}

--eval CREATE USER $user_auth_name@$user_auth_host IDENTIFIED WITH $user_auth_plugin BY $user_auth_pass

if ($user_auth_grant_all)
{
  --eval GRANT ALL ON *.* TO $user_auth_name@$user_auth_host
}

--exec $MYSQLXTEST -u $user_auth_name -p $user_auth_pass --ssl-mode=REQUIRED -e ";" >/dev/null 2>&1

--let $user_auth_name=
--let $user_auth_host=
--let $user_auth_plugin=
--let $user_auth_pass=
--let $user_auth_grant_all=
