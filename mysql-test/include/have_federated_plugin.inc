if (`select plugin_library IS NULL from information_schema.plugins where plugin_name LIKE '%federated%'`)
{
  --skip federated plugin not available
}

