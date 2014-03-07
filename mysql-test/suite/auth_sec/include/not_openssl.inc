-- require r/not_openssl.require
disable_query_log;
show variables like "have_openssl";
enable_query_log;
