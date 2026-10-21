# Cleanup after "cmrs/initialize.inc" script
#
# It removes the component and the fixed internal MRS accounts. Account removal
# is test isolation; component uninstall itself must preserve the accounts.
# Additionally execute standard MRS cleanup.
#

# Use a separate client so this helper preserves mysqltest's query-log state.
--exec $MYSQL --execute="REVOKE MYSQL_REST_SERVICE_ADMIN ON *.* FROM CURRENT_USER()"

UNINSTALL COMPONENT "file://component_mysql_rest_service";

--disable_query_log
--disable_result_log
DROP USER IF EXISTS 'mysql_mrs_metadata'@'localhost',
  'mysql_mrs_data'@'localhost';
--enable_query_log
--enable_result_log

--let $mrs_user_meta=
--let $mrs_user_data=

--let $cleanup_arg_mrs_skip_service_disable=1
--source ../../../router/include/mrs/cleanup.inc
