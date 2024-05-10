#
## Remove MySQL Server accounts that use different authentication
## plugins.
#
# Usage:
#
# [--let $user_cache2=user_name]
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

if ($user_cache2 == "")
{
  --die Required variable not set.
}

#
## Check user hosts
#

if ($host_cache2 == "")
{
  --die Required variable not set.
}

--eval DROP USER $user_cache2@$host_cache2

--let $user_cache2=
--let $host_cache2=
--let $pass_cache2=
