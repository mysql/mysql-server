## Script xplugin_wait_for_connection_closure.inc
#  This scripts waits for all x connections to be closed.
#
## Usage
#
# --source ../include/xplugin_wait_for_connection_closure.inc
#

let $wait_quiet=1;
let $wait_for_status_variable ='Mysqlx_sessions';
let $wait_for_status_value =0;
--source ../include/wait_for_status_variables.inc

let $wait_condition=
  SELECT closed=accepted FROM
      (SELECT VARIABLE_VALUE as closed
          FROM  performance_schema.global_status
          WHERE VARIABLE_NAME = "Mysqlx_connections_closed") as t1,
      (SELECT VARIABLE_VALUE as accepted
          FROM  performance_schema.global_status
          WHERE VARIABLE_NAME = "Mysqlx_connections_accepted") as t2;
--source include/wait_condition.inc

if (!$success)
{
  if ($show_xpl_debug_info)
  {
    --source show_xpl_debug_info.inc
  }
  die;
}
