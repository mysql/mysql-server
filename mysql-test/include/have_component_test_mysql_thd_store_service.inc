disable_query_log;

#
# Check if the variable COMPONENT_TEST_MYSQL_THD_STORE_SERVICE is set
#
if (!$COMPONENT_TEST_MYSQL_THD_STORE_SERVICE) {
   --skip test requires the environment variable \$COMPONENT_TEST_MYSQL_THD_STORE_SERVICE to be set (normally done by mtr), see the file plugin.defs
}

enable_query_log;
