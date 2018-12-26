#
# Bug #11766001: ALLOW MULTIPLE --PLUGIN-LOAD OPTIONS
# 
# test multiple consecutive --plugin-load-add options
# success : both test_plugin_server and qa_auth_server 
#   should be present
SELECT PLUGIN_NAME, PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS
WHERE PLUGIN_NAME IN ('test_plugin_server', 'qa_auth_server')
ORDER BY 1;
PLUGIN_NAME	PLUGIN_STATUS
qa_auth_server	ACTIVE
test_plugin_server	ACTIVE
SELECT * FROM mysql.plugin ORDER BY name;
name	dl
