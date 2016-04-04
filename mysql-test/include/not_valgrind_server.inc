--require r/not_valgrind_server.require
--disable_query_log
eval select $VALGRIND_SERVER_TEST as using_valgrind_server;
--enable_query_log
