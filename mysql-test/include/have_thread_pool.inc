if(`SELECT count(*) = 0 FROM information_schema.plugins
     WHERE PLUGIN_LIBRARY LIKE 'thread_pool%'`)
{
  --skip Test requires: 'thread_pool' plugin loaded
}

