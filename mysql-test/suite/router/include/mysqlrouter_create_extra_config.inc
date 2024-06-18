# ==== Purpose ====
#
# Create additional configuration file for MySQL Router
#
# Note: The script write the file to the disc, and sets
# the `mysqltest` variable to command line argument that
# should appended to MySQL Router while starting (to apply
# new configuration).
#
# ==== Usage ====
#
# [--let $extra_http_port = TCP PORT
# [--let $extra_http_enable_ssl = 1|0]
# [--let $extra_http_static_folder = /some/folder/]]
#
# [--let $extra_keyring_master_file = FILE]
# [--let $extra_keyring_file = FILE]
# [--let $extra_dynamic_state_file = FILE]
#
# --source include/mysqlrouter_restart.inc
#

--source test_dir.inc
--let $SCHEMA_DIR=$TEST_DIR/../include/json_schema/

--disable_query_log
SET @variable_extra_config = "";

if ($extra_router_config)
{
    --inc $extra_router_config
}
# Else
if (! $extra_router_config)
{
  --let $extra_router_config = 1
}

--let $extra_generated_file=generated_router_$extra_router_config.cnf
--let $STD_DATA_DIR=$MYSQL_TEST_DIR/std_data/

--echo # Generating config into $extra_generated_file
if ($extra_http_port)
{
    SET @variable_extra_config = CONCAT(@variable_extra_config,"[http_server]\n");
    --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"port=$extra_http_port\n");
    if ($extra_http_enable_ssl)
    {
        SET @variable_extra_config = CONCAT(@variable_extra_config,"ssl=1\n");
        --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"ssl_cert=$STD_DATA_DIR/server-cert.pem\n");
        --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"ssl_key=$STD_DATA_DIR/server-key.pem\n");
        --echo #  * https
    }

    if ($extra_http_static_folder)
    {
        --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"static_folder=$extra_http_static_folder\n");
    }

    if (! $extra_http_enable_ssl)
    {
        --echo #  * http
    }
}

if ($extra_mrs_meta_user) {
    SET @variable_extra_config = CONCAT(@variable_extra_config,"[mysql_rest_service]\n");
    --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"mysql_user=$extra_mrs_meta_user\n");
    if ($extra_mrs_data_user) {
        --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"mysql_user_data_access=$extra_mrs_data_user\n");
    }
    
    if ($extra_mrs_router_id) {
        --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"router_id=$extra_mrs_router_id\n");
    }
    
    if (!$extra_mrs_read_write_route)
    {
        --die Missing $extra_mrs_read_write_route, this parameter is required.
    }
    
    --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"mysql_read_write_route=$extra_mrs_read_write_route\n");
    
    if ($extra_mrs_read_only_route)
    {
        --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"mysql_read_only_route=$extra_mrs_read_only_route\n");
    }
    
    if ($extra_mrs_metadata_refresh_interval)
    {
        --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"metadata_refresh_interval=$extra_mrs_metadata_refresh_interval\n");
    }

    --echo #  * MySQL Rest Service
}

--let $router_config_to_remove=$MYSQL_TMP_DIR/$extra_generated_file
--source ../include/mysqlrouter_remember_file_for_removal.inc

output $MYSQL_TMP_DIR/$extra_generated_file;
replace_result @variable_extra_config #@variable_extra_config;
SELECT @variable_extra_config;

--let $extra_append_router_config=$extra_append_router_config -a $MYSQL_TMP_DIR/$extra_generated_file

SET @variable_extra_config = "";
--enable_query_log
