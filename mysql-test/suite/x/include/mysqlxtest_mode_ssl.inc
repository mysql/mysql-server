--echo
--echo ############################################################
--echo ## Test starts MySQL Server without configured SSL
--echo ##
--echo ## * Verify client ssl-mode vs MySQL Server without SSL

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=OFF --ssl-mode=disabled  2>&1

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=OFF --ssl-mode=preferred  2>&1

--exec $MYSQLXTEST_SSLCA_TEST --expect-error ER_X_CAPABILITIES_PREPARE_FAILED $USE_SOCKET -v%EXPECT%=OFF --ssl-mode=required  2>&1

--exec $MYSQLXTEST_SSLCA_TEST --expect-error ER_X_CAPABILITIES_PREPARE_FAILED $USE_SOCKET -v%EXPECT%=OFF --ssl-mode=verify_ca  2>&1

--exec $MYSQLXTEST_SSLCA_TEST --expect-error ER_X_CAPABILITIES_PREPARE_FAILED $USE_SOCKET -v%EXPECT%=OFF --ssl-mode=verify_identity  2>&1

--echo
--echo ############################################################
--echo ## Restart MySQL Server with configured SSL and certificate
--echo ## that can be verified
--echo ##
--echo ## * Verify client ssl-mode vs MySQL Server with SSL

 --replace_regex /restart\:.*\.pem/$SSL_VERIFICABLE$/
let $restart_parameters=restart: 
	--mysqlx_ssl_key=$MYSQL_TEST_DIR/std_data/server-key-verify-pass.pem
	--mysqlx_ssl_cert=$MYSQL_TEST_DIR/std_data/server-cert-verify-pass.pem
	--mysqlx_ssl_ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify.pem;
--source restart_mysqld_and_wait_for_xplugin.inc

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=OFF --ssl-mode=disabled  2>&1

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=preferred  2>&1

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=required  2>&1

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=verify_ca  2>&1

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=verify_identity  2>&1

--exec $MYSQLXTEST_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=required  2>&1

# Expect CR_X_TLS_WRONG_CONFIGURATION error
--exec $MYSQLXTEST_TEST --expect-error 2503 $USE_SOCKET -v%EXPECT%=ON --ssl-mode=verify_ca  2>&1

--echo
--echo ############################################################
--echo ## Restart MySQL Server with configured SSL and certificate
--echo ## that can't be verified
--echo ##
--echo ## * Verify client ssl-mode vs MySQL Server with SSL

--replace_regex /restart\:.*\.pem/$SSL_NOT_VERIFICABLE$/
let $restart_parameters=restart: 
	--mysqlx_ssl_key=$MYSQL_TEST_DIR/std_data/server-key-verify-fail.pem
	--mysqlx_ssl_cert=$MYSQL_TEST_DIR/std_data/server-cert-verify-fail.pem
	--mysqlx_ssl_ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify.pem;
--source restart_mysqld_and_wait_for_xplugin.inc

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=OFF --ssl-mode=disabled  2>&1

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=preferred  2>&1

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=required  2>&1

--exec $MYSQLXTEST_SSLCA_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=verify_ca  2>&1

--exec $MYSQLXTEST_SSLCA_TEST --expect-error CR_SSL_CONNECTION_ERROR $USE_SOCKET -v%EXPECT%=ON --ssl-mode=verify_identity  2>&1

--exec $MYSQLXTEST_TEST $USE_SOCKET -v%EXPECT%=ON --ssl-mode=required  2>&1

# Expect CR_X_TLS_WRONG_CONFIGURATION error
--exec $MYSQLXTEST_TEST --expect-error 2503 $USE_SOCKET -v%EXPECT%=ON --ssl-mode=verify_ca  2>&1
