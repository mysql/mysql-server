# ==== Purpose ====
#
# The purpose of this .inc file is to print the output of
# SHOW GRANTS FOR * statement, but in a concise and readable manner.
#
# For example, if we execute SHOW GRANTS FOR root@locahost, we
# get an output like this:
#
# GRANT SELECT, INSERT, ... ON *.* TO `root`@`localhost` WITH GRANT OPTION
# GRANT ALLOW_NONEXISTENT_DEFINER, ... ON *.* TO `root`@`localhost` WITH GRANT OPTION
#
# The output above is huge and makes the .result file harder to read. With this .inc
# file, we will have the output like this:
#
# GRANT <ALL_STATIC_PRIVILEGES> ON *.* TO `root`@`localhost` WITH GRANT OPTION
# GRANT <ALL_DYNAMIC_PRIVILEGES> ON *.* TO `root`@`localhost` WITH GRANT OPTION
#
# ==== Usage ====
#
# Set the $user variable first in the actual test file.
# Then simply --source include/show_grants.inc just below the above line.
#
# Example:
# --let $user = mysqltest_1@localhost
# --source include/show_grants.inc
#

if (!$user)
{
--source include/show_all_privileges.inc
SHOW GRANTS;
}

if($user)
{
--source include/show_all_privileges.inc
eval SHOW GRANTS FOR $user;
}

if($reset_user)
{
--let $user = 0
--let $reset_user = 0
}
