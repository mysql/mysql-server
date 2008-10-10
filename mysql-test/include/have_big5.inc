-- require r/have_big5.require
disable_query_log;
show collation like 'big5_chinese_ci';
enable_query_log;
