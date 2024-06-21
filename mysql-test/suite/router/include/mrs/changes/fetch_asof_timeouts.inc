if (!$mrs_sql_changes_variable)
{
  --let $mrs_sql_changes_variable=@asof_timeouts_changes
  --let $mrs_sql_id_variable=@asof_timeouts_changes_id
}

--disable_query_log
--disable_result_log

START TRANSACTION;
eval SET $mrs_sql_changes_variable=(
           SELECT SUM(IF(v is NULL,0,v+0)) FROM (SELECT details->>"$.restAsofNumberOfTimeouts" as v
           FROM mysql_rest_service_metadata.router_status)as sub);
eval SET $mrs_sql_id_variable=(SELECT MAX(id) FROM mysql_rest_service_metadata.router_status);
COMMIT;

--enable_query_log
--enable_result_log

--let $mrs_sql_id_variable=
--let $mrs_sql_changes_variable=
--let $mrs_do_not_wait_at_fetch=
--let $_mrs_not_first_asof_timeout_fetch=1
