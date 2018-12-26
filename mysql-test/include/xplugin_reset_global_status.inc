## Script xplugin_reset_global_status.inc

--disable_query_log
--disable_result_log

if ($dont_reset_global_status_variables != 1)
{
  if ($MYSQLXGLOBALRESET_DIR == '')
  {
    skip Mysqlx global status variables reset component not available.;
  }

  INSTALL COMPONENT "file://component_mysqlx_global_reset";

# If there are active connections, the 'mysqlx_reset_global_status_variables'
# udf is going to fail (return '0'). Lets wait until the command is executed
# successful (all old connections disconnected)
  let $wait_condition= SELECT mysqlx_reset_global_status_variables() = 1;
  source include/wait_condition_or_abort.inc;

  UNINSTALL COMPONENT "file://component_mysqlx_global_reset";
}

--let $dont_reset_global_status_variables = 0
--enable_query_log
--enable_result_log

