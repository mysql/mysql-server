-- require r/have_openssl_1.require
disable_query_log;
show variables like "have_openssl";
enable_query_log;
