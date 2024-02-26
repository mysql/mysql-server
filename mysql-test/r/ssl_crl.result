# test --crl for the client : should connect
LENGTH(VARIABLE_VALUE) > 0
1
# test --crlpath for the client : should connect
LENGTH(VARIABLE_VALUE) > 0
1
# try logging in with a certificate in the server's --ssl-crl : should fail
#
# Bug#21920678: SSL-CA DOES NOT ACCEPT ~USER TILDE HOME DIRECTORY
#               PATH SUBSTITUTION
#
# try to connect with '--ssl-crl' option using tilde home directoy
# path substitution : should connect
Variable_name	Value
Ssl_cipher	SSL_CIPHER
# try to connect with '--ssl-crlpath' option using tilde home directoy
# path substitution : should connect
Variable_name	Value
Ssl_cipher	SSL_CIPHER
