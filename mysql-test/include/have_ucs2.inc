-- require r/have_ucs2.require
disable_query_log;
show collation like 'ucs2_general_ci';
enable_query_log;
