if (`select count(*) = 0 from information_schema.plugins where plugin_name = 'test_plugin_server' and plugin_status='active'`)
{
  --skip Needs auth_test_plugin
}
