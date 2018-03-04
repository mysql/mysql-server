## Test cases for mysqlx plugin TLS versions

--source include/xplugin_create_user.inc

SET GLOBAL mysqlx_connect_timeout = 300;

eval CREATE USER user5_mysqlx@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'auth_string'
            REQUIRE SSL;

GRANT ALL ON *.* TO user5_mysqlx@localhost;

## Test starts here

--let $xtest_file= $MYSQL_TMP_DIR/connection_tls_version.tmp
--write_file $xtest_file
-->quiet
-->sql
SELECT CONNECTION_TYPE from performance_schema.threads where processlist_command='Query';
SHOW VARIABLES LIKE 'tls_version';
SHOW STATUS LIKE 'Mysqlx_ssl_version';
-->endsql
EOF

--exec $MYSQLXTEST -ux_root --file=$xtest_file 2>&1

--let $XTESTPARAMS= -u user5_mysqlx --password='auth_string' --file=$xtest_file --ssl-cipher='DHE-RSA-AES256-SHA'
--let $ERROR1= /in main, line 0:ERROR: error:00000001:lib\(0\):func\(0\):reason\(1\)/Application terminated with expected error: protocol version mismatch/
--let $ERROR5= /in main, line 0:ERROR: error:00000005:lib\(0\):func\(0\):DH lib/Application terminated with expected error: socket layer receive error/ /in main, line 0:ERROR: error:00000001:lib\(0\):func\(0\):reason\(1\)/Application terminated with expected error: socket layer receive error/

--exec $MYSQLXTEST                                     $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1,TLSv1.2 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1         $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.2         $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1.1,TLSv1.2       $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1                 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1.1               $XTESTPARAMS 2>&1

--let $expected_error_msg= TLS version is invalid \(code 2026\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST --tls-version=TLSv3                 $XTESTPARAMS 2>&1


--let $restart_parameters = restart: --tls-version=TLSv1
--source ../include/restart_mysqld_and_wait_for_xplugin.inc

--exec $MYSQLXTEST                                     $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1,TLSv1.2 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1         $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.2         $XTESTPARAMS 2>&1

--let $expected_error_extra= $ERROR1
--let $expected_error_msg= protocol version mismatch \(code 2026\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST --tls-version=TLSv1.1,TLSv1.2       $XTESTPARAMS 2>&1

--exec $MYSQLXTEST --tls-version=TLSv1                 $XTESTPARAMS 2>&1

--let $expected_error_extra= $ERROR1
--let $expected_error_msg= protocol version mismatch \(code 2026\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST --tls-version=TLSv1.1               $XTESTPARAMS 2>&1

--let $expected_error_msg= TLS version is invalid \(code 2026\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST --tls-version=TLSv3                 $XTESTPARAMS 2>&1


--let $restart_parameters = restart: --tls-version=TLSv1.1
--source ../include/restart_mysqld_and_wait_for_xplugin.inc

--exec $MYSQLXTEST                                     $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1,TLSv1.2 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1         $XTESTPARAMS 2>&1

--let $expected_error_extra= $ERROR5
--let $expected_error_msg= socket layer receive error \(code 2026\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.2         $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1.1,TLSv1.2       $XTESTPARAMS 2>&1

--let $expected_error_extra= $ERROR5
--let $expected_error_msg= socket layer receive error \(code 2026\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST --tls-version=TLSv1                 $XTESTPARAMS 2>&1

--exec $MYSQLXTEST --tls-version=TLSv1.1               $XTESTPARAMS 2>&1

--let $expected_error_msg= TLS version is invalid \(code 2026\)
--source ../include/mysqlxtest_expected_error.inc
--exec $MYSQLXTEST --tls-version=TLSv3                 $XTESTPARAMS 2>&1


# Postamble
--source include/xplugin_drop_user.inc
--remove_file $xtest_file

DROP USER user5_mysqlx@localhost;


