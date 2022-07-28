--source include/allowed_ciphers.inc

--echo # test --crl for the client : should connect
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--replace_regex $ALLOWED_CIPHERS_REGEX
--exec $MYSQL --ssl-mode=VERIFY_CA --ssl-ca=$MYSQL_TEST_DIR/std_data/crl-ca-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/crl-client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/crl-client-cert.pem test --ssl-crl=$MYSQL_TEST_DIR/std_data/crl-client-revoked.crl -e "SELECT LENGTH(VARIABLE_VALUE) > 0 FROM performance_schema.session_status WHERE variable_name = 'ssl_version';"

--echo # test --crlpath for the client : should connect
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--replace_regex $ALLOWED_CIPHERS_REGEX /$MYSQL_TEST_DIR/MYSQL_TEST_DIR/
--exec $MYSQL --ssl-mode=VERIFY_CA --ssl-ca=$MYSQL_TEST_DIR/std_data/crl-ca-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/crl-client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/crl-client-cert.pem --ssl-crlpath=$MYSQL_TEST_DIR/std_data/crldir test -e "SELECT LENGTH(VARIABLE_VALUE) > 0 FROM performance_schema.session_status WHERE variable_name = 'ssl_version';"

--echo # try logging in with a certificate in the server's --ssl-crl : should fail
--error 1
--exec $MYSQL --ssl-mode=VERIFY_CA --ssl-ca=$MYSQL_TEST_DIR/std_data/crl-ca-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/crl-client-revoked-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/crl-client-revoked-cert.pem test -e "SELECT LENGTH(VARIABLE_VALUE) > 0 FROM performance_schema.session_status WHERE variable_name = 'ssl_version';"

--echo #
--echo # Bug#21920678: SSL-CA DOES NOT ACCEPT ~USER TILDE HOME DIRECTORY
--echo #               PATH SUBSTITUTION
--echo #

--let $mysql_test_dir_path= `SELECT REPLACE('$MYSQL_TEST_DIR', '$HOME', '~')`

--echo # try to connect with '--ssl-crl' option using tilde home directoy
--echo # path substitution : should connect
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--replace_regex $ALLOWED_CIPHERS_REGEX
--exec $MYSQL --ssl-ca=$MYSQL_TEST_DIR/std_data/crl-ca-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/crl-client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/crl-client-cert.pem test --ssl-crl=$mysql_test_dir_path/std_data/crl-client-revoked.crl -e "SHOW STATUS LIKE 'Ssl_cipher'"

--echo # try to connect with '--ssl-crlpath' option using tilde home directoy
--echo # path substitution : should connect
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--replace_regex $ALLOWED_CIPHERS_REGEX
--exec $MYSQL --ssl-ca=$MYSQL_TEST_DIR/std_data/crl-ca-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/crl-client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/crl-client-cert.pem --ssl-crlpath=$mysql_test_dir_path/std_data/crldir test -e "SHOW STATUS LIKE 'Ssl_cipher'"
