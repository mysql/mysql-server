# ==== Purpose ====
#
# Reusable configuration of Router with MRS plugin,
# that uses root account to access both meta and user data.
#
# ==== Usage ====
#
# --source ../include/predefined_setup/configure_router_mrs_root.inc
#
--source ../mrs/initialize_schema.inc
--source ./configure_router_mrs_root_inner.inc

--source ../mysqlrouter_mysql_connection.inc

--replace_regex /restart: (.*\/)*/restart:/
--let $restart_parameters= restart: $extra_append_router_config
--source ../mysqlrouter_restart.inc

--let $mrs_host_and_port=127.0.0.1:$extra_http_port
--let MRS_CLIENT_ARGS=$MRS_CLIENT --url https://$mrs_host_and_port
