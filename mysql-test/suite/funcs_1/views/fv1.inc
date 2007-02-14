--disable_query_log
EXECUTE ins_sel_with_result;
SET @select_id = LAST_INSERT_ID();
--enable_query_log

