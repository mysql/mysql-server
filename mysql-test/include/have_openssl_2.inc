-- require r/have_openssl_2.require
disable_query_log;
SHOW STATUS LIKE "Ssl_cipher";
enable_query_log;
