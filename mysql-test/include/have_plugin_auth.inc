if (!`select count(*) FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME='qa_auth_server' and PLUGIN_LIBRARY LIKE 'qa_auth_server%'`) {
  --skip Need the plugin qa_auth_server
}

