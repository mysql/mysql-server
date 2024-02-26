### Trying to connect with ssl-mode as DISABLED. This should establish an unencrypted connection.
RUN SHOW STATUS LIKE 'Mysqlx_ssl_cipher'

Variable_name	Value
Mysqlx_ssl_cipher	
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
### Trying to connect with ssl-mode as REQUIRED. This should establish an encrypted connection.
RUN SHOW STATUS LIKE 'Mysqlx_ssl_cipher'

Variable_name	Value
Mysqlx_ssl_cipher	SSL_CIPHER
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
### Trying to connect with ssl-mode as VERIFY_CA. This should establish an encrypted connection.
RUN SHOW STATUS LIKE 'Mysqlx_ssl_cipher'

Variable_name	Value
Mysqlx_ssl_cipher	SSL_CIPHER
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
### Trying to connect with ssl-mode as VERIFY_IDENTITY. This should establish an encrypted connection.
RUN SHOW STATUS LIKE 'Mysqlx_ssl_cipher'

Variable_name	Value
Mysqlx_ssl_cipher	SSL_CIPHER
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as nonexistent. This should fail.
#Search for the error in the file
Pattern "ERROR: No such host is known 'nonexistent' \(code 2005\)" found
### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as localhost. This should establish an encrypted connection as localhost is present in Alternative Subject Name in the certificate.
RUN SHOW STATUS LIKE 'Mysqlx_ssl_cipher'

Variable_name	Value
Mysqlx_ssl_cipher	SSL_CIPHER
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as 127.0.0.1. This should establish an encrypted connection as localhost is present in Alternative Subject Name in the certificate.
RUN SHOW STATUS LIKE 'Mysqlx_ssl_cipher'

Variable_name	Value
Mysqlx_ssl_cipher	SSL_CIPHER
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
