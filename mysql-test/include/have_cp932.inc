-- require r/have_cp932.require
disable_query_log;
show collation like 'cp932_japanese_ci';
enable_query_log;
