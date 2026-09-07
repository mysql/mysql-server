disable_query_log;

#
# Check if the variable MYSQL_REST_SERVICE_COMPONENT is set
#
if (!$MYSQL_REST_SERVICE_COMPONENT) {
   --skip test requires the environment variable \$MYSQL_REST_SERVICE_COMPONENT to be set (normally done by mtr), see the file plugin.defs
}

enable_query_log;
