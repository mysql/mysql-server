# ==== Purpose ====
#
# Check the behavior of a system variable in both global and session scope.

--let $scope = global
if ($global) {
  if ($dynamic) {
  --source scenarios_for_dynamic_sysvar.inc
  }
  if (!$dynamic) {
  --source scenarios_for_readonly_sysvar.inc
  }
}
if (!$global) {
  --source scenarios_for_wrong_scope_sysvar.inc
}


--let $scope = session
if ($session) {
  if ($dynamic) {
  --source scenarios_for_dynamic_sysvar.inc
  }
  if (!$dynamic) {
  --source scenarios_for_readonly_sysvar.inc
  }
}
if (!$session) {
  --source scenarios_for_wrong_scope_sysvar.inc
}
