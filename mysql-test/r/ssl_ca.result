#
# Bug#21920657: SSL-CA FAILS SILENTLY IF THE PATH CANNOT BE FOUND
#
# try to connect with wrong '--ssl-ca' path : should fail
ERROR 2026 (HY000): SSL connection error: SSL_CTX_set_default_verify_paths failed
# try to connect with correct '--ssl-ca' path : should connect
Variable_name	Value
Ssl_cipher	SSL_CIPHER
#
# Bug#21920678: SSL-CA DOES NOT ACCEPT ~USER TILDE HOME DIRECTORY
#               PATH SUBSTITUTION
#
# try to connect with '--ssl-ca' option using tilde home directoy
# path substitution : should connect
Variable_name	Value
Ssl_cipher	SSL_CIPHER
# try to connect with '--ssl-key' option using tilde home directoy
# path substitution : should connect
Variable_name	Value
Ssl_cipher	SSL_CIPHER
# try to connect with '--ssl-cert' option using tilde home directoy
# path substitution : should connect
Variable_name	Value
Ssl_cipher	SSL_CIPHER
