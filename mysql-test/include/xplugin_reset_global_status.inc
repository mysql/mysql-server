## Script xplugin_reset_global_status.inc

--disable_query_log
--disable_result_log

if ($dont_reset_global_status_variables != 1)
{
  let $wait_condition= SELECT variable_value = 0 FROM performance_schema.global_status WHERE variable_name LIKE 'Mysqlx_sessions';
  source include/wait_condition_or_abort.inc;

  if ($MYSQLXGLOBALRESET_DIR == '')
  {
    skip Mysqlx global status variables reset component not available.;
  }

  INSTALL COMPONENT "file://component_mysqlx_global_reset";
  if (`SELECT mysqlx_reset_global_status_variables() != 1`)
  {
    die Unable to reset global status variables for mysqlx!;
  }
  UNINSTALL COMPONENT "file://component_mysqlx_global_reset";
}

--let $dont_reset_global_status_variables = 0
--enable_query_log
--enable_result_log

