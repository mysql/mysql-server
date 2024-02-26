
let $crllen=`select length(trim(coalesce(@@ssl_crl, ''))) + length(trim(coalesce(@@ssl_crlpath, '')))`;
if (!$crllen)
{
  skip Needs OpenSSL;
}

--echo # test --crl for the client : should connect
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--exec $MYSQL --ssl-mode=VERIFY_CA --ssl-ca=$MYSQL_TEST_DIR/std_data/crl-ca-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/crl-client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/crl-client-cert.pem test --ssl-crl=$MYSQL_TEST_DIR/std_data/crl-client-revoked.crl -e "SELECT LENGTH(VARIABLE_VALUE) > 0 FROM performance_schema.session_status WHERE variable_name = 'ssl_version';"

--echo # test --crlpath for the client : should connect
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--exec $MYSQL --ssl-mode=VERIFY_CA --ssl-ca=$MYSQL_TEST_DIR/std_data/crl-ca-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/crl-client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/crl-client-cert.pem --ssl-crlpath=$MYSQL_TEST_DIR/std_data/crldir test -e "SELECT LENGTH(VARIABLE_VALUE) > 0 FROM performance_schema.session_status WHERE variable_name = 'ssl_version';"

--echo # try logging in with a certificate in the server's --ssl-crlpath : should fail
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--error 1
--exec $MYSQL --ssl-mode=VERIFY_CA --ssl-ca=$MYSQL_TEST_DIR/std_data/crl-ca-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/crl-client-revoked-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/crl-client-revoked-cert.pem test -e "SELECT LENGTH(VARIABLE_VALUE) > 0 FROM performance_schema.session_status WHERE variable_name = 'ssl_version';"
