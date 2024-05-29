if (!$mrs_sql_changes_variable)
{
  --let $mrs_sql_changes_variable=@asof_switches_changes
  --let $mrs_sql_id_variable=@asof_switches_changes_id
}

if (!$mrs_changes_current_is)
{
  --let $mrs_changes_current_is= > $mrs_sql_changes_variable
  if ($mrs_changes_inc)
  {
     --let $mrs_changes_current_is= = ($mrs_changes_inc)
  }
}

--disable_query_log
--disable_result_log

--let $old_value=`SELECT $mrs_sql_changes_variable`
eval SET @_prv_wait_id=$mrs_sql_id_variable;
let $wait_condition=SELECT SUM(IFNULL(details->>"$.restAsofSwitchesFromRo2Rw",0)) $mrs_changes_current_is, $mrs_sql_id_variable:=max(id)
   FROM mysql_rest_service_metadata.router_status
   WHERE id > @_prv_wait_id;
--source include/wait_condition_or_abort.inc

--enable_query_log
--enable_result_log

if ($DEBUG==1)
{
  --let $new_value=`SELECT $mrs_sql_changes_variable`
  --echo Wait files - old limit: $old_value, new limit: $new_value
}

--let $mrs_changes_current_is=
--let $mrs_sql_id_variable=
--let $mrs_schema_id_sql_variable=
if ($_mrs_not_first_asof_swtiches_fetch==1)
{
--let $mrs_sql_changes_variable=
}
--let $mrs_changes_inc=
