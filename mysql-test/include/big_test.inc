--require r/big_test.require
disable_query_log;
eval select $BIG_TEST as using_big_test;
enable_query_log;
