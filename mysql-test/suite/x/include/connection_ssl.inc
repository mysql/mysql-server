## Script connection_ssl.inc
#
# Extracted test logic form 'connection_(wolf|open)ssl.test' and changed to
# a parametric test.
#
# Arguments:
#
#   $INCREMENT  - defines if SSL status variables will be incremented
#   $USE_SOCKET - defines if the test should use UNIX socket (must be set to
#                 MYSQLXTEST argument setting unix socket or must be empty)
#   $USER_AUTH_PLUGIN - defines MySQL Servers account types that is going to
#                 be tested
#   $ERROR_IF_CHANNEL_UNSECURE - defined expected error code for MYSQLXTEST
#                 default connection when test verifies X Plugin logic that
#                 handles "require_secure_transport" system variable.
#
# Usage:
#
# --let $INCREMENT=0|1
# --let $USER_AUTH_PLUGIN=mysql_native_password|sha256_password|caching_sha2_password
# --let $ERROR_IF_CHANNEL_UNSECURE=EXPECTED_ERROR_MESSAGE
# --let $USE_SOCKET=UNIX_SOCKET_CONFIGURATION
# --source ../include/connection_ssl.inc
#

--echo
--echo
--echo ## Preamble
--source ../include/have_performance_schema_threads.inc
--source include/xplugin_preamble.inc
--source include/xplugin_create_user.inc

SET GLOBAL mysqlx_connect_timeout = 300;
call mtr.add_suppression("Maximum number of authentication attempts reached");

## Test starts here
--write_file $MYSQL_TMP_DIR/mysqlx-enable-ssl.tmp
-->import ssl.macro
Mysqlx.Connection.CapabilitiesGet {
}
-->recv capabilities[0]

-->callmacro Enable_ssl_on_xprotocol_and_transmision_layer
-->echo # SSL Enabled

Mysqlx.Connection.CapabilitiesGet {
}
-->recv capabilities[0]

EOF

--write_file $MYSQL_TMP_DIR/mysqlx-setcapabilities-tls-invalid.tmp
-->echo # setting read/write tls param with possible invalid data types:
-->echo # V_SINT,V_UINT ,V_NULL,V_OCTETS ,V_DOUBLE,V_FLOAT,V_STRING
Mysqlx.Connection.CapabilitiesGet {
}
-->recv capabilities[0]

-->echo # Checking CapSet TLS to bool 0
Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type: V_BOOL
          v_bool: 0
        }
      }
    }
  }
}
-->recverror ER_X_CAPABILITIES_PREPARE_FAILED

-->echo # CapSet TLS to null
Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type: V_NULL
        }
      }
    }
 }
}
-->recverror ER_X_CAPABILITIES_PREPARE_FAILED

-->echo # CapSet TLS to empty octet
Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type:V_OCTETS
        }
      }
    }
  }
}
-->recverror ER_X_CAPABILITIES_PREPARE_FAILED

-->echo # CapSet TLS to empty uint
Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type:V_UINT
        }
      }
    }
  }
}
-->recverror ER_X_CAPABILITIES_PREPARE_FAILED

-->echo # CapSet TLS to empty sint
Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type:V_SINT
        }
      }
    }
  }
}
-->recverror ER_X_CAPABILITIES_PREPARE_FAILED
EOF


--write_file $MYSQL_TMP_DIR/verify-ssl-connection-info.tmp
-->import assert_select.macro
-->import assert_status_variable.macro

callmacro Assert_select
	column_user
	%OPTION_CLIENT_USER%@localhost
	SELECT USER() as column_user;

callmacro Assert_select
	CONNECTION_TYPE
	SSL/TLS
	SELECT CONNECTION_TYPE FROM performance_schema.threads WHERE processlist_id = connection_id();

callmacro Assert_status_variable
	Mysqlx_ssl_version
	%OPTION_TLS_VERSION%;

# Use either option with which mysqlxtest was started or the default one: DHE-RSA-AES256-SHA
-->stmtsql SELECT IF(""="%OPTION_SSL_CIPHER%","DHE-RSA-AES256-SHA","%OPTION_SSL_CIPHER%") as Value;
-->recvtovar %EXPECTED_SSL_CIPHER% Value

callmacro Assert_status_variable
	Mysqlx_ssl_cipher
	%EXPECTED_SSL_CIPHER%;

callmacro Assert_status_variable
	Mysqlx_ssl_accepts
	%VAR_SSL_ACCEPTS%;

callmacro Assert_status_variable
	Mysqlx_ssl_finished_accepts
	%VAR_SSL_ACCEPTS%;
EOF


--write_file $MYSQL_TMP_DIR/verify-non-ssl-connection-info.tmp
-->import assert_select.macro
-->import assert_status_variable.macro

callmacro Assert_select
	column_user
	%OPTION_CLIENT_USER%@localhost
	SELECT USER() as column_user;

callmacro Assert_select
	is_non_ssl_con
	1
	SELECT CONNECTION_TYPE in ("TCP/IP", "Socket") as is_non_ssl_con FROM performance_schema.threads WHERE processlist_id = connection_id();

callmacro Assert_status_variable
	Mysqlx_ssl_version
	;

callmacro Assert_status_variable
	Mysqlx_ssl_cipher
	;

callmacro Assert_status_variable
	Mysqlx_ssl_accepts
	%VAR_SSL_ACCEPTS%;

callmacro Assert_status_variable
	Mysqlx_ssl_finished_accepts
	%VAR_SSL_ACCEPTS%;
EOF


# Using "connect-expired-password" Option to allow expired password.

--write_file $MYSQL_TMP_DIR/mysqlx-alter-pwd.tmp
-->sql
ALTER USER USER() IDENTIFIED BY 'alter-new-auth';
-->endsql
EOF


--write_file $MYSQL_TMP_DIR/mysqlx-set-pwd.tmp
-->sql
SET PASSWORD='set-new-auth';
-->endsql
EOF


--write_file $MYSQL_TMP_DIR/mysqlx-status.tmp
-->sql
SHOW STATUS WHERE `Variable_name` RLIKE '^Mysqlx_ssl_(cipher_list|ctx_verify_(depth|mode)|server_not_(after|before)|verify_(depth|mode))$';
-->endsql
EOF

############################################################################

eval CREATE USER user1_mysqlx@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'auth_string1';
eval CREATE USER user2_mysqlx@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'auth_string2'
            PASSWORD EXPIRE;


eval CREATE USER user3_mysqlx@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'auth_string3'
            REQUIRE CIPHER "AES256-SHA" AND
            SUBJECT "/C=SE/ST=Stockholm/L=Stockholm/O=Oracle/OU=MySQL/CN=Client"
            ISSUER "/C=SE/ST=Stockholm/L=Stockholm/O=Oracle/OU=MySQL/CN=CA"
            PASSWORD EXPIRE NEVER;

eval CREATE USER user4_mysqlx@localhost REQUIRE SSL ACCOUNT LOCK;

eval CREATE USER user5_mysqlx@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'auth_string'
            REQUIRE SSL;

eval CREATE USER user6_mysqlx@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY 'dwh@#ghd'
            REQUIRE x509;


eval CREATE USER user7_mysqlx@localhost
            IDENTIFIED WITH $USER_AUTH_PLUGIN BY ''
            REQUIRE CIPHER "AES256-SHA";

GRANT ALL ON *.* TO user1_mysqlx@localhost;
GRANT ALL ON *.* TO user2_mysqlx@localhost;
GRANT ALL ON *.* TO user3_mysqlx@localhost;
GRANT ALL ON *.* TO user4_mysqlx@localhost;
GRANT ALL ON *.* TO user5_mysqlx@localhost;
GRANT ALL ON *.* TO user6_mysqlx@localhost;
GRANT ALL ON *.* TO user7_mysqlx@localhost;

--let $SSL_CONNECTION_MADE=0
--let $CLIENT_SSL_KEY=--ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem
--let $CLIENT_SSL_CERT=--ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem
--let $CLIENT_SSL_CA= --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem

let $MYSQLXTEST=$MYSQLXTEST $USE_SOCKET;
let $MYSQLXTEST_VERIFY_SSL=
   $MYSQLXTEST
	--tls-version=TLSv1
	--file=$MYSQL_TMP_DIR/verify-ssl-connection-info.tmp
	--ssl-mode=REQUIRED
	--quiet;
let $MYSQLXTEST_VERIFY_NON_SSL=
  $MYSQLXTEST
	--file=$MYSQL_TMP_DIR/verify-non-ssl-connection-info.tmp
	--ssl-mode=DISABLED
	--quiet;

--echo
--echo
--echo ## I. Verify handling of SSL with normal account ($USER_AUTH_PLUGIN)
--echo #
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user1_mysqlx --password='auth_string1'
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;


--echo
--echo
--echo ## II. Verify handling of SSL with super account and XProtocol message
--echo ##     CapabilitySet (without SSL options on it)
--echo #
--echo # 1. Try to set "tls" capability with invalid data (no-ssl connection)
--echo # 2. Try to set "tls" capability with valid data (ssl connection)
--echo #
--echo # II.1
exec $MYSQLXTEST -u x_root --password='' -h127.0.0.1
	--no-auth
	--file=$MYSQL_TMP_DIR/mysqlx-setcapabilities-tls-invalid.tmp 2>&1;

--echo # II.2
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST -u x_root --password='' -h127.0.0.1
	--no-auth --ssl-mode=REQUIRED
	--file=$MYSQL_TMP_DIR/mysqlx-enable-ssl.tmp 2>&1;


--echo
--echo
--echo ## III. Verify handling of expired password with user2_mysqlx account ($USER_AUTH_PLUGIN)
--echo #
--echo # 1. When client doesn't support handling of expired password,
--echo #    then connection must be rejected
--echo # 2. When client support expired password, then he must be able
--echo #    to alter password
--echo # 3. Verify that after the alter password, account can be used
--echo #    without any additional support for expired password on the client
--echo # 4. When client support expired password, then he must be able
--echo #    to set the password
--echo # 5. Verify that after the set password, account can be used
--echo #    without any additional support for expired password on the client
--echo #
--echo # III.1
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_MUST_CHANGE_PASSWORD_LOGIN -u user2_mysqlx --password='auth_string2' --plain-auth 2>&1;

--echo # III.2
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST -u user2_mysqlx --password='auth_string2'
	--connect-expired-password --ssl-mode=REQUIRED
	--file=$MYSQL_TMP_DIR/mysqlx-alter-pwd.tmp 2>&1;

--echo # III.3
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user2_mysqlx --password='alter-new-auth'
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;

--echo # III.4
ALTER USER user2_mysqlx@localhost PASSWORD EXPIRE;
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST -u user2_mysqlx --password='alter-new-auth'
	--connect-expired-password --ssl-mode=REQUIRED
	--file=$MYSQL_TMP_DIR/mysqlx-set-pwd.tmp 2>&1;

--echo # III.5
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user2_mysqlx --password='set-new-auth'
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;


--echo
--echo
--echo ## IV. Verify handling of account that has requires concrete cipher, signed cert
--echo ##     ($USER_AUTH_PLUGIN)
--echo #
--echo # 1. Verify that user can connect when it has allowed cipher and matching cert
--echo # 2. Verify that user can't connect when it has allowed cipher and non-matching cert
--echo # 3. Verify that user can't connect when it doesn't use SSL
--echo # 4. Verify that user can't connect when it use basic SSL configuration
--echo # 5. Verify that user can't connect when it has allowed cipher and its without a cert
--echo #
--echo # IV.1
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user3_mysqlx --password='auth_string3'
	--ssl-cipher='AES256-SHA' $CLIENT_SSL_KEY $CLIENT_SSL_CERT
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;

--echo # IV.2
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user3_mysqlx --password='auth_string3' --cached-auth
	--ssl-cipher='AES256-SHA' --ssl-cert=$MYSQL_TEST_DIR/std_data/server-cert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/server-key.pem 2>&1;

--echo # IV.3
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user3_mysqlx --password='auth_string3' --cached-auth 2>&1;

--echo # IV.4
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user3_mysqlx --password='auth_string3' 2>&1;

--echo # IV.5
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user3_mysqlx --password='auth_string3' --cached-auth
	--ssl-cipher='AES256-SHA' 2>&1;


--echo
--echo
--echo ## V. Verify handling of locked account ($USER_AUTH_PLUGIN)
--echo #
--echo # 1. Verify that locked account can't connect
--echo # 2. Unlock the account and verify that it can connect
--echo #
--echo # V.1
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCOUNT_HAS_BEEN_LOCKED -u user4_mysqlx --password=''
	$CLIENT_SSL_KEY $CLIENT_SSL_CERT 2>&1;

--echo # V.2
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
ALTER USER user4_mysqlx@localhost ACCOUNT UNLOCK;
exec $MYSQLXTEST_VERIFY_SSL -u user4_mysqlx --password=''
	$CLIENT_SSL_KEY $CLIENT_SSL_CERT
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;


--echo
--echo
--echo ## VI. Verify handling of account that was marked as SSL required ($USER_AUTH_PLUGIN)
--echo #
--echo # 1. Verify connection with basic SSL configuration
--echo # 2. Verify connection with SSL key & cert
--echo # 3. Verify connection with SSL ca
--echo #
--echo # VI.1
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user5_mysqlx --password='auth_string'
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;

--echo # VI.2
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user5_mysqlx --password='auth_string'
	$CLIENT_SSL_KEY $CLIENT_SSL_CERT
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;

--echo # VI.3
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user5_mysqlx --password='auth_string'
	$CLIENT_SSL_CA
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;


--echo
--echo
--echo ## VII. Verify handling of account that was marked as SSL required with signed cert
--echo ##      ($USER_AUTH_PLUGIN)
--echo #
--echo # 1. Verify connection with signed cert
--echo # 2. Verify that connection is rejected when SSL is disabled
--echo # 3. Verify that connection is rejected when CA is set
--echo # 4. Verify that connection is rejected when using basic SSL configuration
--echo #
--echo # VII.1
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user6_mysqlx --password='dwh@#ghd' --plain-auth
	$CLIENT_SSL_KEY $CLIENT_SSL_CERT
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;

--echo # VII.2
exec $MYSQLXTEST_VERIFY_NON_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user6_mysqlx --password='dwh@#ghd' 2>&1 --cached-auth;

--echo # VII.3
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user6_mysqlx --password='dwh@#ghd' --cached-auth
	$CLIENT_SSL_CA 2>&1;

--echo # VII.4
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user6_mysqlx --password='dwh@#ghd' --plain-auth 2>&1;

--echo
--echo
--echo ## VIII. Verify handling of account that was marked that requires concrete SSL cipher
--echo ##       ($USER_AUTH_PLUGIN)
--echo #
--echo # 1. Verify connection with concrete cipher and signed cert
--echo # 2. Verify that connection is rejected with concrete cipher and without a cert
--echo # 3. Verify that connection is rejected when SSL is disabled
--echo # 4. Verify that connection is rejected with basic SSL configuration
--echo # 5. Verify that connection is rejected with wrong cipher and signed cert
--echo #
--echo # VIII.1
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -u user7_mysqlx
	--ssl-cipher="AES256-SHA" $CLIENT_SSL_CA $CLIENT_SSL_KEY $CLIENT_SSL_CERT
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;

--echo # VIII.2
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user7_mysqlx --cached-auth
  --ssl-cipher="AES256-SHA" 2>&1;

--echo # VIII.3
let $expected_error_msg= Access denied for user 'user7_mysqlx'@'localhost' \(using password: YES\) \(code 1045\);
exec $MYSQLXTEST_VERIFY_NON_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user7_mysqlx --cached-auth 2>&1;

--echo # VIII.4
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user7_mysqlx --cached-auth 2>&1;

--echo # VIII.5
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL --expect-error ER_ACCESS_DENIED_ERROR -u user7_mysqlx --cached-auth
	--ssl-cipher='DHE-RSA-AES256-SHA' $CLIENT_SSL_CA $CLIENT_SSL_KEY $CLIENT_SSL_CERT 2>&1;

--echo
--echo
--echo ## IX. Verify X Plugins global and session status variables ($USER_AUTH_PLUGIN)
--echo #
--echo # 1. Verify global SSL setup
--echo # 2. Verify session SSL setup
--echo #
--echo # IX.1
replace_regex /4294967295/-1/ /18446744073709551615/-1/;
SHOW GLOBAL STATUS WHERE `Variable_name` RLIKE '^Mysqlx_ssl_(cipher_list|ctx_verify_(depth|mode)|server_not_(after|before)|verify_(depth|mode))$';

--echo # IX.2
replace_regex /4294967295/-1/ /18446744073709551615/-1/;
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST -u x_root
	--ssl-cipher="AES256-SHA" $CLIENT_SSL_CA $CLIENT_SSL_KEY $CLIENT_SSL_CERT
	--file=$MYSQL_TMP_DIR/mysqlx-status.tmp 2>&1;


--echo
--echo
--echo ## X. Lets confirm that Mysqlx plugin validates the connection_type vs 'require_secure_transport'
--echo ##    system variable ($USER_AUTH_PLUGIN)
--echo #
--echo # 1. Connection without SSL (fails on TCP, it works on UNIX socket)
--echo # 2. Connection with SSL (always must be successful)
--echo #
--echo # X.1
SET GLOBAL require_secure_transport:=1;
exec $MYSQLXTEST_VERIFY_NON_SSL --expect-error $ERROR_IF_CHANNEL_UNSECURE -ux_root --password=''
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;

--echo # X.2
expr $SSL_CONNECTION_MADE= $SSL_CONNECTION_MADE + $INCREMENT;
exec $MYSQLXTEST_VERIFY_SSL -ux_root --password=''
	$CLIENT_SSL_CA $CLIENT_SSL_KEY $CLIENT_SSL_CERT
	-v%VAR_SSL_ACCEPTS%=$SSL_CONNECTION_MADE 2>&1;
SET GLOBAL require_secure_transport:=0;


--echo
--echo
--echo ## Cleanup
--remove_file $MYSQL_TMP_DIR/verify-ssl-connection-info.tmp
--remove_file $MYSQL_TMP_DIR/verify-non-ssl-connection-info.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-alter-pwd.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-set-pwd.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-setcapabilities-tls-invalid.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-status.tmp
--remove_file $MYSQL_TMP_DIR/mysqlx-enable-ssl.tmp

--source include/xplugin_drop_user.inc

DROP USER user1_mysqlx@localhost;
DROP USER user2_mysqlx@localhost;
DROP USER user3_mysqlx@localhost;
DROP USER user4_mysqlx@localhost;
DROP USER user5_mysqlx@localhost;
DROP USER user6_mysqlx@localhost;
DROP USER user7_mysqlx@localhost;

SET GLOBAL mysqlx_connect_timeout = DEFAULT;

