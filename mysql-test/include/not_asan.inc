-- require r/not_asan.require
disable_query_log;
select (version() like '%asan%') as asan;
enable_query_log;
