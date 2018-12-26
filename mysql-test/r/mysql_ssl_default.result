#
# WL#7712 Support SSL by default in libmysql
#
# verify that mysql default connect with ssl channel when using TCP/IP
# connection
Variable_name	Value
Ssl_cipher	SSL_CIPHER
# verify that mysql --ssl=0 connect with unencrypted channel
Variable_name	Value
Ssl_cipher	
# verify that mysql --ssl=1 connect with ssl channel
Variable_name	Value
Ssl_cipher	SSL_CIPHER
CREATE USER u1@localhost IDENTIFIED BY 'secret' REQUIRE SSL;
# verify that mysqladmin default connect with ssl channel
mysqladmin: [Warning] Using a password on the command line interface can be insecure.
mysqld is alive
# verify that mysql_show default connect with ssl channel
mysqlshow: [Warning] Using a password on the command line interface can be insecure.
+--------------------+
|     Databases      |
+--------------------+
| information_schema |
| test               |
+--------------------+
DROP USER u1@localhost;
