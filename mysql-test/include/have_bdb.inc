-- require r/have_bdb.require
disable_query_log;
show variables like "have_bdb";
enable_query_log;
