#
## Remove MySQL Server accounts that use different authentication
## plugins.
#
# Usage:
#
# [--let $user_native=user_name]
# [--let $user_sha256=user_name]
# [--let $user_cache2=user_name]
#
# [--let $host_native=host_name_or_ip]
# [--let $host_sha256=host_name_or_ip]
# [--let $host_cache2=host_name_or_ip]
#
# --source ../include/remove_mysql_accounts_for_xplugin_tests.h
#
# Example:
# --source ../include/create_mysql_accounts_for_xplugin_tests.h
# .
# . Test case
# .
# # $user_* and $host_* variables were set by
# # create_mysql_accounts_for_xplugin_tests.h. There is no need
# # to set them directly.
# --source ../include/remove_mysql_accounts_for_xplugin_tests.h
#

#
## Check user names
#

if ($user_native == "")
{
  --die Required variable not set.
}

if ($user_sha256 == "")
{
  --die Required variable not set.
}

if ($user_cache2 == "")
{
  --die Required variable not set.
}

#
## Check user hosts
#

if ($host_native == "")
{
  --die Required variable not set.
}

if ($host_sha256 == "")
{
  --die Required variable not set.
}

if ($host_cache2 == "")
{
  --die Required variable not set.
}

--eval DROP USER $user_native@$host_native
--eval DROP USER $user_sha256@$host_sha256
--eval DROP USER $user_cache2@$host_cache2

--let $user_native=
--let $user_sha256=
--let $user_cache2=

--let $host_native=
--let $host_sha256=
--let $host_cache2=

--let $pass_native=
--let $pass_sha256=
--let $pass_cache2=
