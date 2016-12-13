## Script xplugin_wait_for_interfaces.inc
#
#  $do_not_wait_for_x_interface -
#    set for which interfaces the script shouldn't wait
#    In default the script wait for both socket and tcp
#    on *NIX OS-es and only for TCP on windows
#
## Usage
#
# --let $wait_for_tcpsocket_status = valid_value|undefined_value|no_wait
# --let $wait_for_unixsocket_status = valid_value|undefined_value|no_wait
# --source ../include/xplugin_wait_for_interfaces.inc
#

if ($wait_for_tcpsocket_status != "no_wait")
{
  --let $wait_operation= !=
  if ($wait_for_tcpsocket_status == "undefined_value")
  {
    --let $wait_operation= =
  }

  let $wait_condition=
    SELECT 1 FROM performance_schema.global_status WHERE
      VARIABLE_NAME like "Mysqlx_address" AND
      VARIABLE_VALUE != "" AND
      VARIABLE_VALUE $wait_operation "UNDEFINED";
  --source include/wait_condition_or_abort.inc
}

if ($wait_for_unixsocket_status != "no_wait")
{
  --let $wait_operation= !=
  if ($wait_for_unixsocket_status == "undefined_value")
  {
    --let $wait_operation= =
  }

  let $wait_condition=
    SELECT 1 FROM performance_schema.global_status WHERE
      (VARIABLE_NAME like "Mysqlx_socket" AND
       VARIABLE_VALUE != "" AND
       VARIABLE_VALUE $wait_operation "UNDEFINED") OR
      CONVERT(@@version_compile_os using latin1) IN ("Win32","Win64","Windows");
  --source include/wait_condition_or_abort.inc
}

--let $wait_for_tcpsocket_status=
--let $wait_for_unixsocket_status=
