# ==== Purpose ====
#
# The replication suites are often run in multiple 'combinations',
# e.g. with binlog_format=row, mixed, or statement.  Some specific
# test cases do not care about this variable, but need to run with
# multiple values of some *other* variable, which the test framework
# does not explicitly vary.
#
# This file is used to replace one variable combination by another.
#
# Notice that there is currently no other way to run a test with
# a non-standard 'combination', and verify that the result file is
# the same for all cases.
#
# ==== Usage ====
#
# --let $replace_combination_from= {SESSION|GLOBAL|BOTH}.VARIABLE_NAME=VAL1,VAL2[,VAL3[,VAL4...]]
# --let $replace_combination_to= {SESSION|GLOBAL|BOTH}.VARIABLE_NAME=VAL1,VAL2[,VAL3[,VAL4...]]
# [--let $replace_on_all_servers= 1]
# --source include/begin_replace_combination.inc
#
# ... test test test ...
#
# --source include/end_replace_combination.inc
#
# Parameters:
#   $replace_combination_from
#     Variable that the test suite varies, and a comma-separated list
#     of expected values.  This variable will be set to the first
#     value in the list.  It will be set in the specified scopes: only
#     global, only session, or both.
#
#   $replace_combination_to
#     Variable that this script will vary instead of
#     $replace_combination_from.  If the variable in
#     $replace_combination_from takes the Nth value from the value
#     list in $replace_combination_from, then the variable in
#     $replace_combination_to will take the Nth value in the value
#     list in $replace_combination_to.  The variable will be set in
#     the specified scopes: only global, only session, or both.
#
#   $replace_combination_on_all_servers
#     Use rpl_for_each_server_stmt.inc to set the variable on all
#     servers.  This requires previous use of master-slave.inc or
#     rpl_init.inc.
#
# Notes:
# - The *session* variable is only set on the current connection when
#   $replace_combinations_on_all_servers=0. It is only set on the
#   connection server_N (where N=1,2...), when
#   $replace_combinations_on_all_servers=1.


--let $include_filename= begin_replace_combination.inc [$replace_combination_from -> $replace_combination_to]
--source include/begin_include_file.inc

--disable_query_log

# Extract scopes, variables, and value lists from script parameters.
--let $_rc_from_variable_scope= `SELECT SUBSTRING_INDEX('$replace_combination_from', '.', 1)`
--let $_rc_to_variable_scope= `SELECT SUBSTRING_INDEX('$replace_combination_to', '.', 1)`
--let $_rc_from_variable= `SELECT SUBSTRING_INDEX(SUBSTR('$replace_combination_from', LENGTH('$_rc_from_variable_scope') + 2), '=', 1)`
--let $_rc_to_variable= `SELECT SUBSTRING_INDEX(SUBSTR('$replace_combination_to', LENGTH('$_rc_to_variable_scope') + 2), '=', 1)`
--let $_rc_from_value_list= `SELECT SUBSTR('$replace_combination_from', LENGTH('$_rc_from_variable_scope.$_rc_from_variable') + 2)`
--let $_rc_to_value_list= `SELECT SUBSTR('$replace_combination_to', LENGTH('$_rc_to_variable_scope.$_rc_to_variable') + 2)`

# Get current value of 'from' variable and translate to new value of
# 'to' variable.
--let $_rc_from_value= `SELECT @@GLOBAL.$_rc_from_variable`
--let $_rc_from_value_index= `SELECT FIND_IN_SET('$_rc_from_value', '$_rc_from_value_list')`
if ($_rc_from_value_index == 0)
{
  --echo Variable '$_rc_from_variable' has value '$_rc_from_value' which is not listed in '$_rc_from_value_list'
  --die 'From' variable has a value that is not among the listed values
}
--let $_rc_to_value= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$_rc_to_value_list', ',', $_rc_from_value_index), ',', -1)`

# Backup old 'to' value and compute new 'from' value.
--let $_rc_new_from_value= `SELECT SUBSTRING_INDEX('$_rc_from_value_list', ',', 1)`
--let $_rc_old_to_value= `SELECT @@GLOBAL.$_rc_to_variable`

# Build SQL statements.
--let $rpl_sql= SET
if ($_rc_to_variable_scope != SESSION)
{
  --let $rpl_sql= $rpl_sql @@GLOBAL.$_rc_to_variable = '$_rc_to_value',
}
if ($_rc_to_variable_scope != GLOBAL)
{
  --let $rpl_sql= $rpl_sql @@SESSION.$_rc_to_variable = '$_rc_to_value',
}
if ($_rc_from_variable_scope != SESSION)
{
  --let $rpl_sql= $rpl_sql @@GLOBAL.$_rc_from_variable = '$_rc_new_from_value'
  if ($_rc_from_variable_scope == BOTH)
  {
   --let $rpl_sql= $rpl_sql,
  }
}
if ($_rc_from_variable_scope != GLOBAL)
{
  --let $rpl_sql= $rpl_sql @@SESSION.$_rc_from_variable = '$_rc_new_from_value'
}

# Execute SQL statements.
if ($replace_combination_on_all_servers)
{
  --source include/rpl_for_each_server_stmt.inc
}
if (!$replace_combination_on_all_servers)
{
  --eval $rpl_sql
}


--let $include_filename= begin_replace_combination.inc [$replace_combination_from -> $replace_combination_to]
--source include/end_include_file.inc
