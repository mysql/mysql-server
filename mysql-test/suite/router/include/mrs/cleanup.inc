if (!$KEEP_ROUTER_FILES)
{

if ($extra_mrs_router_id)
{
   --let $_expected_result=0
   # The query does two things, checks if GR is installed. If yes, then it checks if we use MultiMaster mode.
   # In case of MM, we are stopping MRS, because it will lead to mixing DDL and DML at cleanup.
   if (`select count(*) > 0 and MAX(VARIABLE_VALUE)="OFF" from performance_schema.global_variables where VARIABLE_NAME="group_replication_single_primary_mode";`)
   {
     if (!$cleanup_arg_mrs_existing_service)
     {
       --die The varialbe 'cleanup_arg_mrs_existing_service' is needed in multi-master GR mode, for proper MRS shutdown.
     }

     --let $mrs_modify_enabled=false
     --source config/modify_enable.inc

     # Reset arguments for mrs_client, set by the test run.
     --let $mrs_client_exe_args=
     --let $mrs_client_arg_payload=
     --let $mrs_client_arg_json_pointer=
     --let $mrs_client_arg_exclude_json_pointer=
     --let $mrs_client_arg_request_type=
     --let $mrs_client_arg_write_format=
     --let $mrs_client_arg_write_to_file=
     --let $mrs_client_arg_session_file=

     # The http-plugin responses with HTML, the mrs hook overwrites
     # http reponse from 404 html to 404 json. Thus after stopping
     # MRS we will receive plain html.
     --let $mrs_client_arg_dispaly=none
     --let $mrs_client_arg_response_type=raw
     --let $mrs_client_arg_path='$cleanup_arg_mrs_existing_service'
     --let $mrs_client_arg_expected_status=404
     --let $mrs_client_arg_wait_until_status=60
     --source mrs_client.inc
   }
}


if ($router_test_schema)
{
   eval DROP SCHEMA $router_test_schema;
}
DROP SCHEMA mysql_rest_service_metadata;
DROP ROLE mysql_rest_service_admin;
DROP ROLE mysql_rest_service_schema_admin;
DROP ROLE mysql_rest_service_meta_provider;
DROP ROLE mysql_rest_service_data_provider;
DROP ROLE mysql_rest_service_dev;

if ($router_config_to_remove1)
{
  --remove_file $router_config_to_remove1
}

if ($router_config_to_remove2)
{
  --remove_file $router_config_to_remove2
}

if ($router_config_to_remove3)
{
  --remove_file $router_config_to_remove3
}

if ($restart_router_id)
{
--force-rmdir $MYSQL_TMP_DIR/router.$restart_router_id/data/
}

}


if ($KEEP_ROUTER_FILES == 1)
{
if ($router_test_schema)
{
   --echo DROP SCHEMA $router_test_schema;
}
--echo DROP SCHEMA mysql_rest_service_metadata;
--echo DROP ROLE mysql_rest_service_admin;
--echo DROP ROLE mysql_rest_service_schema_admin;
--echo DROP ROLE mysql_rest_service_meta_provider;
--echo DROP ROLE mysql_rest_service_data_provider;
--echo DROP ROLE mysql_rest_service_dev;
}

--let $cleanup_arg_mrs_existing_service=
