-- require r/have_debug.require
disable_query_log;
select instr(version(),convert('debug' using utf8))!=0;
enable_query_log;
