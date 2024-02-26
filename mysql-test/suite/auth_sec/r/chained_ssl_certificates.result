call mtr.add_suppression("Server SSL certificate doesn't verify");
CREATE USER 'user1'@'%' REQUIRE SSL;
Variable_name	Value
Ssl_cipher	SSL_CIPHER
Variable_name	Value
Ssl_cipher	SSL_CIPHER
Variable_name	Value
Ssl_cipher	SSL_CIPHER
Restart server and provide same ssl-ca at server startup
Variable_name	Value
Ssl_cipher	SSL_CIPHER
Restart server and provide same ssl-ca at server startup
ERROR 2026 (HY000): SSL connection error: error:ERRCODE:SSL routines:ERRMSG:certificate verify failed
DROP USER 'user1';
