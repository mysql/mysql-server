#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip validate password requires dynamic loading
}

#
# Check if the variable VALIDATE_PASSWORD is set
#
if (!$VALIDATE_PASSWORD) {
  --skip validate password requires the environment variable \$VALIDATE_PASSWORD to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for validate password
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$VALIDATE_PASSWORD_OPT/'`) {
  --skip validate password requires that --plugin-dir is set to the validate password dir (either the .opt file does not contain \$VALIDATE_PASSWORD_OPT or another plugin is in use)
}
