# ==== Purpose ====
#
# Reusable configuration of Router with MRS plugin,
# that uses root account to access both meta and user data.
#
# ==== Usage ====
#
# --source ../include/predefined_setup/configure_router_mrs_root.inc
#

--source ../mrs_initialize_schema.inc

--let $mrs_user_meta=root@localhost
--let $mrs_user_data=root@localhost
--source ../mrs_user_configure.inc

--let $keyring_operation=init
--let $keyring_set_path_and_value=root password ""
--source ../mysqlrouter_keyring.inc

--let $extra_http_port=`SELECT $ROUTER_PORT + 1`
--let $extra_http_enable_ssl=0
--let $extra_mrs_meta_user=root
--let $extra_mrs_read_write_route=undertest
--let $extra_keyring_master_file=$keyring_master_file
--let $extra_keyring_file=$keyring_file
--source ../mysqlrouter_create_extra_config.inc

--source ../mysqlrouter_mysql_connection.inc

--replace_regex /restart: (.*\/)*/restart:/
--let $restart_parameters= restart: $extra_append_router_config
--source ../mysqlrouter_restart.inc
