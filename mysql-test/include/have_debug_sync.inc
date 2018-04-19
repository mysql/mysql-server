let $have_debug_sync= query_get_value(SHOW VARIABLES LIKE 'debug_sync', Value, 1);
if ($have_debug_sync == 'No such row')
{
  --skip Test requires 'have_debug_sync'
}
