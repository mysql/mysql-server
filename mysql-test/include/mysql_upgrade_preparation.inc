# Include this in any test using mysql_upgrade


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

#
# Preserve the original timestamps in:
# - table mysql.user
# - table mysql.tables_priv
# for the mysql.sys user.
#

--disable_query_log
--disable_result_log

--let $saved_sys_timestamp_1 = `SELECT password_last_changed FROM mysql.user WHERE Host='localhost' AND User='mysql.sys'`

--let $saved_sys_timestamp_2 = `SELECT Timestamp from mysql.tables_priv WHERE Host='localhost' AND Db='sys' AND User='mysql.sys' AND Table_name='sys_config'`

--enable_result_log
--enable_query_log

