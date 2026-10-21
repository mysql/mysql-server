if ($MTR_ROUTER_MRS_USE_COMPONENT)
{
  if ($_mrs_component_installed)
  {
    --disable_query_log
    --disable_result_log
    UNINSTALL COMPONENT "file://component_mysql_rest_service";

    DROP USER IF EXISTS 'mysql_mrs_metadata'@'localhost',
      'mysql_mrs_data'@'localhost';
    --enable_result_log
    --enable_query_log

    --let $_mrs_component_installed=0
  }

  --let $cleanup_arg_mrs_skip_service_disable=1
  --let $mrs_user_meta=
  --let $mrs_user_data=
}

if (!$KEEP_ROUTER_FILES)
{

if ($_mrs_started)
{
  --exec $MRS_CLIENT_ARGS --path /debug?do=stop --wait-until-status=60 --display none
  --let $_mrs_started=0
}

if ($extra_mrs_router_id)
{
   --let $_expected_result=0
   # Disable the MRS service and wait for the MRS to notice it
   if (!$cleanup_arg_mrs_skip_service_disable)
   {
    if (!$cleanup_arg_mrs_existing_service)
    {
      --let $cleanup_arg_mrs_existing_service=/index.html
    }
     --let $mrs_modify_enabled=false
     --source config/modify_enable.inc

     # The http-plugin responses with HTML, the mrs hook overwrites
     # http reponse from 404 html to 404 json. Thus after stopping
     # MRS we will receive plain html.
     --let $mrs_client_arg_display=none
     --let $mrs_client_arg_response_type=raw
     --let $mrs_client_arg_path='$cleanup_arg_mrs_existing_service'
     --let $mrs_client_arg_expected_status=404
     --let $mrs_client_arg_wait_until_status=60
     --source mrs_client.inc
   }
}

let $schemas_to_remove=$router_test_schemas;
while($schemas_to_remove)
{
  let $schema=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$schemas_to_remove", ";", 1),"\n",""))`;
  let $schemas_to_remove=`SELECT TRIM(SUBSTRING("$schemas_to_remove", LENGTH("$schema") + 2))`;

  eval DROP SCHEMA $schema;
}
--let $router_test_schemas=

DROP SCHEMA mysql_rest_service_metadata;

  DROP ROLE mysql_rest_service_admin;
  DROP ROLE mysql_rest_service_schema_admin;
if ($cleanup_keep_roles != 1)
{
  DROP ROLE mysql_rest_service_meta_provider;
  DROP ROLE mysql_rest_service_data_provider;
}

  DROP ROLE mysql_rest_service_dev;


if ($mrs_schema_version >= 4)
{
# This may be executed on tests that run schema 2,3,4 and it
# would produce the difference in `result` file.
# This output must be silenced.

--disable_query_log
--disable_result_log
DROP ROLE mysql_rest_service_user;
--enable_result_log
--enable_query_log
}


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
let $schemas_to_remove=$router_test_schemas;
while($schemas_to_remove)
{
  let $schema=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$schemas_to_remove", ";", 1),"\n",""))`;
  let $schemas_to_remove=`SELECT TRIM(SUBSTRING("$schemas_to_remove", LENGTH("$schema") + 2))`;

  --echo DROP SCHEMA $schema;
}
--echo DROP SCHEMA mysql_rest_service_metadata;

  --echo DROP ROLE mysql_rest_service_admin;
  --echo DROP ROLE mysql_rest_service_schema_admin;
if ($cleanup_keep_roles != 1)
{
  --echo DROP ROLE mysql_rest_service_meta_provider;
  --echo DROP ROLE mysql_rest_service_data_provider;
}
  --echo DROP ROLE mysql_rest_service_dev;
}

--enable_warnings

--let $cleanup_arg_mrs_existing_service=
--let $$cleanup_arg_mrs_skip_service_disable=
--let $cleanup_keep_roles=
