if (!$KEEP_ROUTER_FILES)
{

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

if ($keyring_master_file)
{
--remove_file $keyring_master_file
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