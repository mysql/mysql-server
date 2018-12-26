
if (!`select count(*) FROM INFORMATION_SCHEMA.PLUGINS
      WHERE PLUGIN_NAME='qa_auth_server'
      and PLUGIN_LIBRARY LIKE 'qa_auth_server%'`)
{
  --skip Need the plugin qa_auth_server
}

--echo #
--echo # Bug #11766001: ALLOW MULTIPLE --PLUGIN-LOAD OPTIONS
--echo # 

--echo # test multiple consecutive --plugin-load options
--echo # success : only qa_auth_server should be present
SELECT PLUGIN_NAME, PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS
  WHERE PLUGIN_NAME IN ('test_plugin_server', 'qa_auth_server')
  ORDER BY 1;
