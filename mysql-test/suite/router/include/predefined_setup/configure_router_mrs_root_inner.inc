# Initialize basic setup with one server and static routing.
#

--let $mrs_user_meta=root@localhost
--let $mrs_user_data=root@localhost
--source ../mrs/user_configure.inc

--let $extra_http_port=`SELECT $ROUTER_PORT + 1`
if (!$extra_http_enable_ssl)
{
  if ($extra_http_enable_ssl != 0)
  {
    --let $extra_http_enable_ssl=1
  }
}
--let $extra_mrs_meta_user=root
if (!$extra_mrs_read_write_route)
{
  --let $extra_mrs_read_write_route=undertest
}
--let $extra_keyring_master_file=$keyring_master_file
--let $extra_keyring_file=$keyring_file
--let $extra_dynamic_state_file=$MYSQL_TMP_DIR/state.json
--source ../mysqlrouter_create_extra_config.inc

