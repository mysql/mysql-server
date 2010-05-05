if (`select plugin_library IS NULL from information_schema.plugins where plugin_name LIKE '%blackhole%'`)
{
  --skip blackhole plugin not available;
}

