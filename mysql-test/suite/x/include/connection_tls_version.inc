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
SELECT CONNECTION_TYPE from performance_schema.threads where PROCESSLIST_ID=CONNECTION_ID();
SHOW VARIABLES LIKE 'tls_version';
SHOW STATUS LIKE 'Mysqlx_ssl_version';
-->endsql
EOF

--exec $MYSQLXTEST -ux_root --file=$xtest_file 2>&1

--let $XTESTPARAMS= -u user5_mysqlx --password='auth_string' --file=$xtest_file --ssl-cipher='ECDHE-RSA-AES128-GCM-SHA256'
# each of the below regexps has 2 replaces
# 1: for openssl 1.0
# 2: for openssl 1.1
# 3: for freebsd openssl 1.0
--let $ERROR1= /error:14077102:SSL routines:SSL23_GET_SERVER_HELLO:unsupported protocol/protocol version mismatch/ /error:14171102:SSL routines:tls_process_server_hello:unsupported protocol/protocol version mismatch/ /record layer version error/protocol version mismatch/ /error:1425F102:SSL routines:ssl_choose_client_version:unsupported protocol/protocol version mismatch/
--let $ERROR5= /error:00000000:lib\(0\):func\(0\):reason\(0\)/socket layer receive error/ /error:1409442E:SSL routines:ssl3_read_bytes:tlsv1 alert protocol version/socket layer receive error/

--exec $MYSQLXTEST                                     $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1,TLSv1.2 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1,TLSv1.1         $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.2         $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1.1,TLSv1.2       $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1                 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1.1               $XTESTPARAMS 2>&1

--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv3 $XTESTPARAMS 2>&1


--let $restart_parameters = restart: --tls-version=TLSv1.2
--source ../include/restart_mysqld_and_wait_for_xplugin.inc

--exec $MYSQLXTEST                                     $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1,TLSv1.2 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1,TLSv1.1         $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.2         $XTESTPARAMS 2>&1

--replace_regex $ERROR1
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1.1,TLSv1.2 $XTESTPARAMS 2>&1

--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1                               $XTESTPARAMS 2>&1

--replace_regex $ERROR1
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1.1 $XTESTPARAMS 2>&1

--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv3   $XTESTPARAMS 2>&1


--let $restart_parameters = restart: --tls-version=TLSv1.2
--source ../include/restart_mysqld_and_wait_for_xplugin.inc

--exec $MYSQLXTEST                                     $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1,TLSv1.1,TLSv1.2 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1,TLSv1.1         $XTESTPARAMS 2>&1

--replace_regex $ERROR5
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1,TLSv1.2 $XTESTPARAMS 2>&1
--exec $MYSQLXTEST --tls-version=TLSv1.1,TLSv1.2                   $XTESTPARAMS 2>&1

--replace_regex $ERROR5
--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1 $XTESTPARAMS 2>&1

--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv1.1                   $XTESTPARAMS 2>&1

--exec $MYSQLXTEST --expect-error CR_SSL_CONNECTION_ERROR --tls-version=TLSv3 $XTESTPARAMS 2>&1


# Cleanup
--let $restart_parameters = restart:
--source include/restart_mysqld.inc

--source include/xplugin_drop_user.inc
--remove_file $xtest_file

DROP USER user5_mysqlx@localhost;
SET GLOBAL mysqlx_connect_timeout = DEFAULT;
