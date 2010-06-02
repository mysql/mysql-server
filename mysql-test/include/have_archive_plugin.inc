if (`select plugin_library IS NULL from information_schema.plugins where plugin_name LIKE '%archive%'`)
{
  --skip archive plugin not available
}

