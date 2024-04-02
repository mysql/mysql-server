# ==== Purpose ====
#
# TODO
#
# Note: ...
#
# ==== Usage ====
#
# [--let $... = TCP PORT
#
#
# --source include/SCRIPT.inc
#

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

if (! $extra_metadata_user)
{
  --die Variable 'extra_metadata_user' is required.
}

if (! $extra_rw_route)
{
  --die Variable 'extra_rw_route' is required.
}

if (! $extra_route_rw_port)
{
  --die Variable 'extra_route_rw_port' is required.
}

--echo # Generating config into $extra_generated_file

--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"[metadata_cache:bootstrap]\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"cluster_type=gr\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"router_id=1\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"address=127.0.0.1\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"auth_cache_refresh_interval=4\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"ttl=10\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"bootstrap_server_addresses=mysql://127.0.0.1:$SERVER_MYPORT_1\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"user=$extra_metadata_user\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"metadata_cluster=my_cluster\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"auth_cache_ttl=-1\n");


--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"[routing:$extra_rw_route]\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"bind_address=0.0.0.0\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"destinations=metadata-cache://my_cluster/?role=PRIMARY\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"bind_port=$extra_route_rw_port\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"routing_strategy=first-available\n");
--eval SET @variable_extra_config = CONCAT(@variable_extra_config,"protocol=classic\n");

--echo #  * router: $extra_rw_route

if ($extra_ro_route)
{
   if (! $extra_route_ro_port)
   {
     --die Variable 'extra_route_ro_port' is required, when ro route was configured.
   }
   
  --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"[routing:$extra_ro_route]\n");
  --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"bind_address=0.0.0.0\n");
  --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"destinations=metadata-cache://my_cluster/?role=SECONDARY\n");
  --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"bind_port=$extra_route_ro_port\n");
  --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"routing_strategy=first-available\n");
  --eval SET @variable_extra_config = CONCAT(@variable_extra_config,"protocol=classic\n");
  
  --echo #  * router: $extra_ro_route
}

--let $router_config_to_remove=$MYSQL_TMP_DIR/$extra_generated_file
--source ../include/mysqlrouter_remember_file_for_removal.inc

output $MYSQL_TMP_DIR/$extra_generated_file; 
replace_result @variable_extra_config #@variable_extra_config;
SELECT @variable_extra_config;

--let $extra_append_router_config=$extra_append_router_config -a $MYSQL_TMP_DIR/$extra_generated_file

SET @variable_extra_config = "";
--enable_query_log
