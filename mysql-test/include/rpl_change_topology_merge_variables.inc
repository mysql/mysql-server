# ==== Purpose ====
#
# The purpose of this utility script is to override the values of a
# topology include typical parameter of the form `1:2, 3:a, 4:off, *:on,
# ...` with the values of another topology script parameter, when the
# values for the latter match a specific value.
#
# ==== Usage ====
#
# --let $rpl_topology_override_variable =
# --let $rpl_topology_with_variable =
# --let $rpl_topology_when_override_value =
# --let $rpl_server_count =
# --source include/save_sysvars.inc
#
# Parameters:
#
#  $rpl_topology_override_variable
#    The variable to be overriden, on the form `1:2, 3:a, 4:off, *:on, ...`
#
#  $rpl_topology_with_variable
#    The variable to get the override values from, on the form `1:2, 3:a,
#    4:off, *:on, ...`
#
#  $rpl_topology_when_override_value
#    The value to look for in `$rpl_topology_with_variable` to trigger an
#    override in `$rpl_topology_override_variable`
#
#  $rpl_server_count
#    Number of servers in the topology
#
# Output:
#
#  $rpl_topology_overriden_result
#    The result from overriden the `$rpl_topology_override_variable` with
#    the values in `$rpl_topology_with_variable` that match
#    `$rpl_topology_override_value`
#
#

if ($rpl_topology_with_variable == '') {
  --die ERROR IN TEST: The $rpl_topology_with_variable parameter must be set
}

if ($rpl_topology_when_override_value == '') {
  --die ERROR IN TEST: The $rpl_topology_when_override_value parameter must be set
}

if ($rpl_server_count == '') {
  --die ERROR IN TEST: The $rpl_server_count parameter must be set
}

--let $_overriden_variable =

--let $_override_index = `SELECT LOCATE('*:$rpl_topology_when_override_value', '$rpl_topology_with_variable')`
--let $_overriden_index = `SELECT LOCATE('*:', '$rpl_topology_override_variable')`
if ($_override_index) {
  --let $_override_value = `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_topology_with_variable', $_override_index), ',', 1), ':', -1)`
  --let $_overriden_variable = *:$_override_value
}
if (`SELECT NOT($_override_index) AND $_overriden_index`) {
  --let $_override_value = `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_topology_override_variable', $_overriden_index), ',', 1), ':', -1)`
  --let $_overriden_variable = *:$_override_value
}

--let $_conn_count = $rpl_server_count
while($_conn_count != 0) {
  --let $_override_index = `SELECT LOCATE('$_conn_count:$rpl_topology_when_override_value', '$rpl_topology_with_variable')`
  --let $_overriden_index = `SELECT LOCATE('$_conn_count:', '$rpl_topology_override_variable')`

  if ($_override_index) {
    if ($_overriden_variable != '') {
      --let $_overriden_variable = $_overriden_variable,
    }
    --let $_override_value = `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_topology_with_variable', $_override_index), ',', 1), ':', -1)`
    --let $_overriden_variable = $_overriden_variable $_conn_count:$_override_value
  }
  if (`SELECT NOT($_override_index) AND $_overriden_index`) {
    if ($_overriden_variable != '') {
      --let $_overriden_variable = $_overriden_variable,
    }
    --let $_override_value = `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_topology_override_variable', $_overriden_index), ',', 1), ':', -1)`
    --let $_overriden_variable = $_overriden_variable $_conn_count:$_override_value
  }

  --dec $_conn_count
}

--let $rpl_topology_overriden_result = $_overriden_variable
