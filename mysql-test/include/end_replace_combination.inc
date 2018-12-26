# See begin_replace_combination.inc.

--let $include_filename= end_replace_combination.inc [$replace_combination_from -> $replace_combination_to]
--source include/begin_include_file.inc


--disable_query_log

# Build SQL statements.
--let $rpl_sql= SET
if ($_rc_to_variable_scope != SESSION)
{
  --let $rpl_sql= $rpl_sql @@GLOBAL.$_rc_to_variable = '$_rc_old_to_value',
}
if ($_rc_to_variable_scope != GLOBAL)
{
  --let $rpl_sql= $rpl_sql @@SESSION.$_rc_to_variable = '$_rc_old_to_value',
}
if ($_rc_from_variable_scope != SESSION)
{
  --let $rpl_sql= $rpl_sql @@GLOBAL.$_rc_from_variable = '$_rc_from_value'
  if ($_rc_from_variable_scope == BOTH)
  {
   --let $rpl_sql= $rpl_sql,
  }
}
if ($_rc_from_variable_scope != GLOBAL)
{
  --let $rpl_sql= $rpl_sql @@SESSION.$_rc_from_variable = '$_rc_from_value'
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


--let $include_filename= end_replace_combination.inc [$replace_combination_from -> $replace_combination_to]
--source include/end_include_file.inc
