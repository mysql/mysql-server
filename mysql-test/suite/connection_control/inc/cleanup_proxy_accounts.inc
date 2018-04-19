disable_query_log;
disable_result_log;
# Revoke proxy grants
REVOKE PROXY ON proxied@localhost FROM u1@localhost, u2@localhost, u3@localhost;

# Drop proxy users
DROP USER u1@localhost, u2@localhost, u3@localhost;

# Drop proxied user
DROP USER proxied@localhost;

# Uninstall test_plugin_server
UNINSTALL PLUGIN test_plugin_server;

# Remove plugin library
let $auth_plugin_path= `SELECT SUBSTR('$PLUGIN_AUTH_OPT/$PLUGIN_AUTH', 14)`;
let $connection_control_plugin_path= `SELECT SUBSTR('$CONNECTION_CONTROL_PLUGIN_OPT/$PLUGIN_AUTH', 14)`;

if ($auth_plugin_path != $connection_control_plugin_path)
{
  --remove_file $connection_control_plugin_path
}
enable_result_log;
enable_query_log;
