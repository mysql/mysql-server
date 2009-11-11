--require r/have_debug_sync.require
disable_query_log;
let $value= query_get_value(SHOW VARIABLES LIKE 'debug_sync', Value, 1);
eval SELECT ('$value' LIKE 'ON %') AS debug_sync;
enable_query_log;
