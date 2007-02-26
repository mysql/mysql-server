-- require r/one_thread_per_connection.require
disable_query_log;
select @@thread_handling;
enable_query_log;

