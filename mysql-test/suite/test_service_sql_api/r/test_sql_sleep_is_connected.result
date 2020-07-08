------ Run plugin ------------------------------------------------
INSTALL PLUGIN test_sql_sleep_is_connected SONAME 'TEST_SQL_SLEEP_IS_CONNECTED';
------ Stop plugin -----------------------------------------------
UNINSTALL PLUGIN test_sql_sleep_is_connected;
------ plugin log ------------------------------------------------
=========================================================================
Opening Session
-------------------------------------------------------------------------
Test interaction between `sleep` and `is_connected`, in case when connection break at 1 call to is_connected
Execute: SELECT SLEEP(3600);
 > sql_field_metadata: SLEEP(3600)
 > sql_connection_alive => returns false
 > sql_get_longlong
 > sql_handle_ok
-------------------------------------------------------------------------
Test interaction between `sleep` and `is_connected`, in case when connection break at 2 call to is_connected
Execute: SELECT SLEEP(3600);
 > sql_field_metadata: SLEEP(3600)
 > sql_connection_alive => returns true
 > sql_connection_alive => returns false
 > sql_get_longlong
 > sql_handle_ok
-------------------------------------------------------------------------
Test interaction between `sleep` and `is_connected`, in case when connection break at 4 call to is_connected
Execute: SELECT SLEEP(3600);
 > sql_field_metadata: SLEEP(3600)
 > sql_connection_alive => returns true
 > sql_connection_alive => returns true
 > sql_connection_alive => returns true
 > sql_connection_alive => returns false
 > sql_get_longlong
 > sql_handle_ok
-------------------------------------------------------------------------
Test sleep and is_connected interactions, in case when connection never breaks
Execute: SELECT SLEEP(20);
 > sql_field_metadata: SLEEP(20)
 > sql_get_longlong
 > sql_handle_ok
 > is_connected was called several times.
=========================================================================
Close Session
------ cleanup ---------------------------------------------------
