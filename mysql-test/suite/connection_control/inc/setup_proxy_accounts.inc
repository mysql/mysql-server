disable_query_log;
disable_result_log;
# Copy PLUGIN_AUTH library to CONNECTION_CONTROL_OPT location
let $auth_plugin_path= `SELECT SUBSTR('$PLUGIN_AUTH_OPT/$PLUGIN_AUTH', 14)`;
let $connection_control_plugin_path= `SELECT SUBSTR('$CONNECTION_CONTROL_PLUGIN_OPT/$PLUGIN_AUTH', 14)`;

if ($auth_plugin_path != $connection_control_plugin_path)
{
  --error 0, 1
  --remove_file $connection_control_plugin_path
  --copy_file $auth_plugin_path $connection_control_plugin_path
}
# Install test_plugin_server
eval INSTALL PLUGIN test_plugin_server SONAME '$PLUGIN_AUTH';

# Create proxied@localhost
CREATE USER proxied@localhost IDENTIFIED BY 'proxied_password';

# Create u1@localhost, u2@localhost, u3@localhst
CREATE USER u1@localhost IDENTIFIED WITH test_plugin_server AS 'proxied';
CREATE USER u2@localhost IDENTIFIED WITH test_plugin_server AS 'proxied';
CREATE USER u3@localhost IDENTIFIED WITH test_plugin_server AS 'proxied';

GRANT PROXY ON proxied@localhost TO u1@localhost, u2@localhost, u3@localhost;
enable_result_log;
enable_query_log;
