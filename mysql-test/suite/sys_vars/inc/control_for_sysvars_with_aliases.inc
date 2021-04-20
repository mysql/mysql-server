# ==== Purpose ====
#
# Check the behavior of all system variables that have an alias.
#
# ==== Requirements ====
#
# For dynamic variables, see
# mysql-test/suite/sys_vars/t/check_dynamic_sysvar.inc
#
# For readonly varialbes, see
# mysql-test/suite/sys_vars/t/check_readonly_sysvar.inc
#
# ==== Implementation ====
#
# This is part of a framework of several files. The framework is
# organized in three layers:
#
# - Control layer: this iterates over all system variables. For each
#   variable it chooses a set of scenarios to execute, depending on
#   the properties of the variable.
#
#   The control layer is split into the following files:
#
#   - control_for_sysvars_with_aliases.inc
#     Iterate over variables. For each variable, invoke
#     control_for_session_and_global_sysvar.inc twice: first for the
#     variable and then for the alias.
#
#   - control_for_sysvar.inc
#     Check the properties of the variable: whether it exists on
#     session/global scope, and whether it is dynamic or readonly.
#     Invoke a file in the scenario layer according to the outcome.
#
# - Scenario layer: this executes a number of scenarios.  Each file in
#   this layer executes scenarios relevant for a certain kind of
#   variable: a dynamic (settable) variable, a read-only variable, or
#   a variable that does not exist in the given scope.  The files are:
#
#   - scenarios_for_dynamic_sysvar.inc
#     Execute the following scenarios for a dynamic sysvar:
#     - The default is as expected
#     - SET ... = DEFAULT works
#     - SET ... = <value> works, for all listed values
#     - For numeric variables, the range is as specified
#     - SET ... = <value> fails, for all listed invalid values.
#
#   - scenarios_for_readonly_sysvar.inc
#     Execute the following scenarios for a readonly sysvar:
#     - The default is as expected
#     - SET ... = DEFAULT fails
#     - SET ... = <value> fails, for all listed values
#
#   - scenarios_for_wrong_scope_sysvar.inc
#     Execute the following scenarios for a sysvar that is not defined
#     on the given scope:
#     - SET ... = DEFAULT fails
#     - SET ... = <value> fails, for all listed values
#     - SELECT ... fails
#     - If the scope is global, the variable does not exist in the
#       performance_schema.global_variables.
#     - If the given scope is session, the variable exists in the
#       performance_schema.session_variables table (which is according
#       to the specification for that table; it shall list global
#       variables that do not have a session counterpart).
#
#   The files in the scenario layer typically invoke files in the
#   assertion layer, for each scenario that they setup.
#
#   - assert_sysvar_value.inc
#     Asserts that the variable has the expected values...
#     - when accessed as @@variable,
#     - in performance_schema.global_variables and/or
#       performance_schema.session_variables,
#     - in performance_schema.persisted_variables,
#     - in the persisted variables file.
#     Asserts that the variable has the expected source in
#     performance_schema.variables_info.
#
#   - assert_sysvar_and_alias_value.inc
#     Invokes assert_sysvar_value.inc for the variable, and then
#     invokes assert_sysvar_value.inc for its alias.
#
#   - assert_sysvar_warnings.inc
#     Asserts that SHOW WARNINGS returns the expected set of warnings
#     (possibly none).


# This test checks the behavior of configuration variables.  If the
# test is invoked with some of those variables set to specific values,
# that interferes with the test.  So we disable runs that set the
# tested variables.

# Binlog-less runs use --skip-log-replica-updates.
--source include/have_log_bin.inc


--echo #### Initialize ####

--let $assert_escape = 1

--source json_sysvar_spec.inc

--let $json_label = value
--source include/create_json_iterator.inc

--let $json_label = invalid
--source include/create_json_iterator.inc

--echo #### Run ####

--let $json_array = $sysvar_specs
--source $json_sysvar_start
while (!$json_sysvar_done) {
  if ($mask_value) {
    --echo - default=MASKED
    --echo - values=MASKED
  }
  if (!$mask_value) {
    --echo - default=$default
    --echo - values=$values
  }

  --source control_for_sysvar.inc

  # Above, we ran a number of test scenarios where we *SET* $name, and
  # *checked* both $name and $alias.
  # Below, we run a number of test scenarios where we *SET* $alias,
  # and *check* both $alias and $name (by swapping the values of $name
  # and $alias here).
  if ($alias) {
    --let $expect_deprecation_warning = 1
    --let $tmp = $alias
    --let $alias = $name
    --let $name = $tmp
    --source control_for_sysvar.inc
    --let $expect_deprecation_warning = 0
  }

  --source $json_sysvar_next
}

--echo #### Clean up ####

--source include/destroy_json_functions.inc
