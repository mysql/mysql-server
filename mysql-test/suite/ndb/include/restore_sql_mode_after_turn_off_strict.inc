# Restore sql_mode
--disable_query_log
set @@sql_mode = @start_sql_mode_saved_value_hj65rf;
--enable_query_log
