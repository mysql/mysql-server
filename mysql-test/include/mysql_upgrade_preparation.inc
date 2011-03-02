# Include this in any test using mysql_upgrade

# Can't run test of external client with embedded server
-- source include/not_embedded.inc

# Only run test if "mysql_upgrade" is found
--require r/have_mysql_upgrade.result
--disable_query_log
select LENGTH("$MYSQL_UPGRADE")>0 as have_mysql_upgrade;
--enable_query_log

#
# Hack:
#
# If running with Valgrind ($VALGRIND_TEST <> 0) then the resource
# consumption (CPU) for upgrading a large log table will be intense.
# Therefore, truncate the log table in advance and issue a statement
# that should be logged.
#
if ($VALGRIND_TEST)
{
  --disable_query_log
  --disable_result_log
  --disable_abort_on_error
  TRUNCATE TABLE mysql.general_log;
  SELECT 1;
  --enable_abort_on_error
  --enable_result_log
  --enable_query_log
}
