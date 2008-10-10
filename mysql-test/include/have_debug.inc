-- require r/have_debug.require
disable_query_log;
select (version() like '%debug%') as debug;
enable_query_log;
