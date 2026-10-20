#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip authentication_webauthn plugin requires dynamic loading
}

#
# Check if the variable WEBAUTHN_AUTHENTICATION_PLUGIN is set
#
if (!$WEBAUTHN_AUTHENTICATION_PLUGIN) {
  --skip authentication_webauthn plugin requires the environment variable \$WEBAUTHN_AUTHENTICATION_PLUGIN to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for authentication_webauthn
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$WEBAUTHN_AUTHENTICATION_PLUGIN_OPT/'`) {
  --skip authentication_webauthn plugin requires that --plugin-dir is set to the authentication_webauthn plugin dir (either the .opt file does not contain \$WEBAUTHN_AUTHENTICATION_PLUGIN_OPT or another plugin is in use)
}
