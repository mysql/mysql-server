--disable_query_log
set @start_sql_mode_saved_value_hj65rf=@@sql_mode;
set sql_mode=(select replace(@@sql_mode,'STRICT_TRANS_TABLES',''));
--enable_query_log
