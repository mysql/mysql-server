# Set before marker
disable_query_log;
disable_result_log;
SET @before= TIMESTAMP(current_time());
enable_result_log;
enable_query_log;
