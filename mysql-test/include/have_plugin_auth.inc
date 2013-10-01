if (!`select count(*) FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME='test_plugin_server' and PLUGIN_LIBRARY LIKE 'auth_test_plugin%'`) {
  --skip Need the plugin test_plugin_server
}

