-- require r/have_openssl_2.require
disable_query_log;
SHOW STATUS LIKE "SSL_get_cipher";
enable_query_log;
