# ==== Purpose ====
#
# Create dynamic state file that the Router requires on start when 
# configred for InnoDB Cluster.
#
# Note: Normally it is created by the bootstrap but this is for the use 
# when we are stubbing the bootstrap step, preparing the Router config 
# manually.
#
# ==== Usage ====
#
# [--let $extra_dynamic_state_file = filepath]
# [--let $s1_port = PORT]
# --source include/mysqlrouter_dynamic_state.inc
#
# Parameters:
#
#   $extra_dynamic_state_file
#     Path to the file that should be used as an output.
#
#   $s1_port
#     Port of the first metadata server of the Cluster.
#
#   $group_replication_group_name
#     Name of the replication group.


--disable_query_log
SET @variable_dynamic_state_json = "{\n";
SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, '"metadata-cache": {\n');

SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, '"group-replication-id": "');
--eval SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, "$group_replication_group_name");
SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, '",\n');

SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, '"cluster-metadata-servers": ["mysql://127.0.0.1:');
--eval SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, "$s1_port");
SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, '"]\n');

SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, "},\n");
SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, '"version": "1.0.0"');
SET @variable_dynamic_state_json = CONCAT(@variable_dynamic_state_json, "}\n");

--let $router_config_to_remove=$extra_dynamic_state_file
--source ../include/mysqlrouter_remember_file_for_removal.inc

output $extra_dynamic_state_file;
replace_result @variable_dynamic_state_json '';
SELECT @variable_dynamic_state_json;
--enable_query_log
