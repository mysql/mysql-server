
if (!$mrs_modify_enabled)
{
  --die `mrs_modify_enabled` variable is required.
}

--disable_query_log
--disable_result_log

eval UPDATE `mysql_rest_service_metadata`.`config` SET service_enabled = $mrs_modify_enabled;

--enable_query_log
--enable_result_log


--let $mrs_modify_enabled=
