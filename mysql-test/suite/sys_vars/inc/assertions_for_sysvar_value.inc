# ==== Purpose ====
#
# 1. Verify that a system variable has the expected value when
#    reading the @@variable.
# 2. Verify that a system variable has the expected value when
#    reading it from the performance_schema table.
# 3. Verify that a system variable has the expected VARIABLE_SOURCE
#    in performance_schema.variables.info.
# 4. Verify that a system variable has the expected value in
#     performance_schema.persisted_variables.
# 5. Verify that a system variable has the expected value in the
#    persisted variables file.
#
# ==== Usage ====
#
# --let $variable = NAME
# --let $scope = {global|session}
# --let $expect_value = VARIABLE_VALUE
# [--let $value_map = JSON_OBJECT]
# [--let $ps_value_map = JSON_OBJECT]
# [--let $mask_value = 1]
# [--let $expect_persisted_ps = {exists|noexists}]
# [--let $expect_persisted_json = {exists|noexists}]
# [--let $persisted_json = JSON_OBJECT | LOAD]
# [--let $expect_source = SOURCE]
# [--let $expect_persisted_value = {VALUE|SAME_AS_VALUE}]
# [--let $rpl_debug = 1]
# --source assertions_for_sysvar_value.inc
#
# Parameters:
#   $variable
#     The name of the variable.
#
#   $scope
#     The scope where the variable should be checked.
#     See also scenarios_for_dynamic_sysvar.inc.
#
#   $value_map, $ps_value_map, $mask_value
#     See json_sysvar_spec.inc
#
#   $expect_value
#     The value that was set for the variable.
#
#   $expect_persisted_ps
#     If set to 'exists', assert that the variable exists in
#     performance_schema.persisted_variables. If set to 'noexists',
#     assert that the variable does not exist in
#     performance_schema.persisted_variables. If not set, perform no
#     such check.
#
#   $expect_persisted_json
#     If set to 'exists', assert that the variable exists in
#     $persisted_json (see below). If set to 'noexists', assert that
#     the variable does not exist in $persisted_json. If not set,
#     perform no such check.
#
#   $persisted_json
#     If $expect_persisted_json is set, this should be set to the full
#     file contents of the persisted variables file, with single
#     quotes and backslashes escaped. Use load_persisted_json.inc. Or
#     set it to LOAD to automatically load the file.
#
#   $expect_source
#     If this is not empty, check that the VARIABLE_SOURCE field in
#     performance_schema.variables_info has this value.
#
#   $expect_persisted_value
#     Expect that the persisted value is this. Use SAME_AS_VALUE if it
#     is expected to be the same as $expect_value.  This will be
#     passed through $value_map and $ps_value_map just like
#     $expect_value.
#
#   $rpl_debug
#     Print extra debug info.

--let $assert_escape = 1

# 1. @@GLOBAL.NAME

# Get "actual" expected value from map.
#
# $expect_value should be the value that was previously set with a SET
# statement.  Usually, you see the same value in SELECT, but not
# always: e.g., after "SET variable = TRUE", "SELECT variable" will
# return 1.  And after "SET variable = 'x'" with quotes around x,
# "SELECT variable" will return x without quotes.  For such cases, the
# caller should include the transformation as an entry in $value_map,
# where the key is the value that was used in SET and the value is
# what you expect to see in SELECT.
--let $json_key = $expect_value
--let $json_object = $value_map
--let $json_lookup_output_single_quote_escaped = 1
--source include/json_lookup.inc
--let $_csv_expect_value = $json_value

# Get value from @@variable
--let $_csv_value = `SELECT @@$scope.$variable`
--let $_csv_value = escape(\',$_csv_value)

# Verify that value is expected
--let $assert_cond = '$_csv_value' = '$_csv_expect_value'
if ($mask_value) {
  --let $assert_text = @@$scope.$variable should have the expected value
}
if (!$mask_value) {
  --let $assert_text = @@$scope.$variable should be $_csv_expect_value
}
--source include/assert.inc

# 2. PERFORMANCE_SCHEMA.GLOBAL_VARIABLES

# Get "actual" expected value from map.
--let $json_key = $_csv_expect_value
--let $json_object = $ps_value_map
--source include/json_lookup.inc
--let $_csv_expect_value_ps = $json_value

# Get value from performance_schema
--let $_csv_table = _variables
--let $_csv_table = $scope$_csv_table
--let $_csv_value_ps = `SELECT VARIABLE_VALUE FROM performance_schema.$_csv_table WHERE VARIABLE_NAME = '$variable'`
--let $_csv_value_ps = escape(\',$_csv_value_ps)

# Verify that value is expected
--let $assert_cond = '$_csv_value_ps' = '$_csv_expect_value_ps'
if ($mask_value) {
  --let $assert_text = $variable should have the expected value in P_S.$_csv_table
}
if (!$mask_value) {
  --let $assert_text = $variable should be $_csv_expect_value in P_S.$_csv_table
}
--source include/assert.inc

# 3. PERFORMANCE_SCHEMA.VARIABLES_INFO/SOURCE

if ($expect_source) {
  --let $assert_cond = '[SELECT VARIABLE_SOURCE FROM performance_schema.variables_info WHERE VARIABLE_NAME = '$variable']' = '$expect_source'
  --let $assert_text = $variable should have source $expect_source in P_S.variables_info
  --source include/assert.inc
}

# 4. PERFORMANCE_SCHEMA.PERSISTED_VARIABLES

# Get expected persisted value (may be different from expected global
# value in case of set persist_only or set global).
--let $_csv_expect_persisted_value = $_csv_expect_value_ps
if ($rpl_debug) {
  --echo DEBUG: assertions_for_sysvar_value: expect_persisted_value=<$expect_persisted_value>
}
if ($expect_persisted_value != SAME_AS_VALUE) {
  --let $json_key = $expect_persisted_value
  --let $json_object = $value_map
  --let $json_lookup_output_single_quote_escaped = 0
  --source include/json_lookup.inc
  --let $json_key = $json_value
  --let $json_object = $ps_value_map
  --let $json_lookup_output_single_quote_escaped = 1
  --source include/json_lookup.inc
  --let $_csv_expect_persisted_value = $json_value
}
if ($rpl_debug) {
  --echo DEBUG: assertions_for_sysvar_value: _csv_expect_persisted_value=<$_csv_expect_persisted_value>
}

if ($expect_persisted_ps == exists) {
  # Get value from performance_schema
  --let $_csv_value_ps = `SELECT VARIABLE_VALUE FROM performance_schema.persisted_variables WHERE VARIABLE_NAME = '$variable'`
  --let $_csv_value_ps = escape(\',$_csv_value_ps)
  --let $assert_cond = '$_csv_value_ps' = '$_csv_expect_persisted_value'
  if ($mask_value) {
    --let $assert_text = $variable should have the expected value in P_S.persisted_variables
  }
  if (!$mask_value) {
    --let $assert_text = $variable should be $_csv_expect_persisted_value in P_S.persisted_variables
  }
  --source include/assert.inc
}
if ($expect_persisted_ps == noexists) {
  # Get count from performance_schema
  --let $_csv_count = `SELECT COUNT(*) FROM performance_schema.persisted_variables WHERE VARIABLE_NAME = '$variable'`
  --let $assert_cond = '$_csv_count' = '0'
  --let $assert_text = $variable should not exist in P_S.persisted_variables
  --source include/assert.inc
}

# 5. PERSISTED VARIABLES FILE

if ($expect_persisted_json != '') {
  # Read JSON from file, if required
  --let $_csv_persisted_json = $persisted_json
  if ($persisted_json == LOAD) {
    --source load_persisted_json.inc
    --let $_csv_persisted_json = $persisted_json
    --let $persisted_json = LOAD
  }
  # Verify that value exists.
  if ($expect_persisted_json == exists) {
    if ($mask_value) {
      --let $assert_text = $variable should have the expected value in the persist file
    }
    if (!$mask_value) {
      --let $assert_text = $variable should have value $_csv_expect_persisted_value in the persist file
    }

    # Variables tagged as "PERSIST_AS_READONLY" are stored in a
    # sub-document.
    if ($persist_as_readonly) {
      --let $subsection = mysql_static_variables
    }
    if (!$persist_as_readonly) {
      --let $subsection = mysql_dynamic_variables
    }
    --let $actual_value = `SELECT JSON_UNQUOTE(JSON_EXTRACT('$_csv_persisted_json', '$.$subsection.$variable.Value'))`
    --let $actual_value = escape(\',$actual_value)
    --let $assert_cond = '$actual_value' = '$_csv_expect_persisted_value'
    --source include/assert.inc
  }
  # Verify that value does not exist.
  if ($expect_persisted_json == noexists) {
    --let $assert_text = $variable should not exist in the persist file
    --let $assert_cond = JSON_CONTAINS_PATH('$_csv_persisted_json', 'one', '$.mysql_dynamic_variables.$variable') = 0
    --source include/assert.inc
  }
}
