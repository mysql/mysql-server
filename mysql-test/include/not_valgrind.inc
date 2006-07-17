--require r/not_valgrind.require
--disable_query_log
eval select $VALGRIND_TEST as using_valgrind;
--enable_query_log
