-- require r/have_query_cache.require
disable_query_log;
show variables like 'have_query_cache';
enable_query_log;
