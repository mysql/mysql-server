# Reset all underlying Performance Schema data

--disable_result_log
--disable_query_log ONCE
CALL sys.ps_truncate_all_tables(FALSE);
--enable_result_log
