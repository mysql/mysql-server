#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip mysql_no_login requires dynamic loading
}

#
# Check if the variable MYSQL_NO_LOGIN is set
#
if (!$MYSQL_NO_LOGIN) {
  --skip mysql_no_login requires the environment variable \$MYSQL_NO_LOGIN to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for mysql_no_login
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$MYSQL_NO_LOGIN_OPT/'`) {
  --skip mysql_no_login requires that --plugin-dir is set to the mysql_no_login dir (either the .opt file does not contain \$MYSQL_NO_LOGIN_OPT or another plugin is in use)
}

