-- require r/have_nodebug.require
disable_query_log;
select (version() like '%debug%') as debug;
enable_query_log;
