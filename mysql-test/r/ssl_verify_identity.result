### Trying to connect with ssl-mode as DISABLED. This should establish an unencrypted connection.
Variable_name	Value
Ssl_cipher	
WARNING: no verification of server certificate will be done. Use --ssl-mode=VERIFY_CA or VERIFY_IDENTITY.
### Trying to connect with ssl-mode as REQUIRED. This should establish an encrypted connection.
Variable_name	Value
Ssl_cipher	SSL_CIPHER
WARNING: no verification of server certificate will be done. Use --ssl-mode=VERIFY_CA or VERIFY_IDENTITY.
### Trying to connect with ssl-mode as VERIFY_CA. This should establish an encrypted connection.
Variable_name	Value
Ssl_cipher	SSL_CIPHER
### Trying to connect with ssl-mode as VERIFY_IDENTITY. This should establish an encrypted connection.
Variable_name	Value
Ssl_cipher	SSL_CIPHER
### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as nonexistent. This should fail.
#Search for the error in the file
Pattern "ERROR 2005 \(HY000\): Unknown MySQL server host 'nonexistent'" found
### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as localhost. This should establish an encrypted connection as localhost is present in Alternative Subject Name in the certificate.
Variable_name	Value
Ssl_cipher	SSL_CIPHER
### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as 127.0.0.1. This should establish an encrypted connection as localhost is present in Alternative Subject Name in the certificate.
Variable_name	Value
Ssl_cipher	SSL_CIPHER
