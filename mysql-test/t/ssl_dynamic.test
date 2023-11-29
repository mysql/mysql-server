
# Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--disable_query_log
# The SSL library may fail initializing during this one
call mtr.add_suppression("Failed to set up SSL because of the following SSL library error");
call mtr.add_suppression("Failed to initialize TLS for channel: mysql_main");
call mtr.add_suppression("CA certificate/certficates is invalid. Please check logs for more details.");
call mtr.add_suppression("Failed to validate certificate .*");
call mtr.add_suppression("Server certificate .* verification has failed. Check logs for more details");
call mtr.add_suppression("Failed to set up TLS. Check logs for details");
call mtr.add_suppression("Internal TLS error error.*");
call mtr.add_suppression("Value for option 'ssl_cipher' contains cipher 'gizmo' that is blocked");
call mtr.add_suppression("Value for option 'tls_ciphersuites' contains cipher 'gizmo' that is blocked");
--enable_query_log

--echo # Check if ssl is on
SELECT LENGTH(VARIABLE_VALUE) > 0 FROM performance_schema.session_status
  WHERE VARIABLE_NAME='Ssl_cipher';

--echo ################## FR1.1 and FR 1.4: ALTER INSTANCE RELOAD TLS

ALTER INSTANCE RELOAD TLS;

--echo # Check if ssl is still turned on after reload
SELECT LENGTH(VARIABLE_VALUE) > 0 FROM performance_schema.session_status
  WHERE VARIABLE_NAME='Ssl_cipher';

--echo # FR1.1: check if old sessions continue
connect (ssl_con,localhost,root,,,,,SSL);

SET @must_be_present= 'present';

connection default;

ALTER INSTANCE RELOAD TLS;

connection ssl_con;

--echo # Success criteria: value must be present
SELECT @must_be_present;

connection default;
disconnect ssl_con;

--echo # cleanup
# Wait until all sessions are disconnected
--source include/wait_until_count_sessions.inc


--echo ################## FR 1.2: check if new sessions get the new vals

--echo # Save the defaults
let $orig_cipher= query_get_value(SHOW STATUS LIKE 'Ssl_cipher', Value, 1);
SET @orig_ssl_cipher = @@global.ssl_cipher;
SET @orig_tls_version = @@global.tls_version;

--echo # in ssl_con
connect (ssl_con,localhost,root,,,,,SSL);

--echo # check if the session has the original values
--replace_result $orig_cipher orig_cipher
SHOW STATUS LIKE 'Ssl_cipher';

--echo # in default connection
connection default;

--echo # setting new values for ssl_cipher
SET GLOBAL ssl_cipher = "ECDHE-RSA-AES256-GCM-SHA384";
SET GLOBAL tls_version = "TLSv1.2";
ALTER INSTANCE RELOAD TLS;

--echo # in ssl_new_con
connect (ssl_new_con,localhost,root,,,,,SSL);
--echo # Save the new defaults
let $new_cipher= query_get_value(SHOW STATUS LIKE 'Ssl_cipher', Value, 1);

--echo # Check if the old and the new not afters differ
let $the_same=`SELECT "$new_cipher" = "$orig_cipher"`;
if ($the_same == 1)
{
  die the not-after values must be different;
}

--echo # in ssl_con
connection ssl_con;

--echo # the con session must have the original values
--replace_result $orig_cipher orig_cipher;
SHOW STATUS LIKE 'Ssl_cipher';

--echo # cleanup
--echo # in default connection
connection default;
disconnect ssl_con;
disconnect ssl_new_con;

SET GLOBAL ssl_cipher = @orig_ssl_cipher;
SET GLOBAL tls_version = @orig_tls_version;
ALTER INSTANCE RELOAD TLS;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc



--echo ################## FR 1.5: new values effective only after RELOAD TLS

--echo # Save the defaults
let $orig_cipher= query_get_value(SHOW STATUS LIKE 'Ssl_cipher', Value, 1);
SET @orig_ssl_cipher = @@global.ssl_cipher;


--echo # setting new values for ssl_cipher
SET GLOBAL ssl_cipher = "ECDHE-RSA-AES128-GCM-SHA256";

--echo # in ssl_con
connect (ssl_con,localhost,root,,,,,SSL);

let $new_cipher= query_get_value(SHOW STATUS LIKE 'Ssl_cipher', Value, 1);

--echo # Check if the old and the new not afters differ
let $the_same=`SELECT "$new_cipher" = "$orig_cipher"`;
if ($the_same == 0)
{
  die the old non-after must still be active;
}

--echo # cleanup
--echo # in default connection
connection default;
disconnect ssl_con;

SET GLOBAL ssl_cipher = @orig_ssl_cipher;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc


--echo ################## FR 1.7: CONNECTION_ADMIN will be required to execute
--echo #  ALTER INSTANCE RELOAD TLS
CREATE USER test_connection_admin@localhost;

--echo # in ssl_con
connect (ssl_con,localhost,test_connection_admin,,,,,SSL);

--echo # Must fail
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER INSTANCE RELOAD TLS;

--echo # in default connection
connection default;
GRANT SUPER ON *.* TO test_connection_admin@localhost;

--echo # in ssl_con
connection ssl_con;

--echo # Must fail
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER INSTANCE RELOAD TLS;

--echo # in default connection
connection default;
REVOKE SUPER ON *.* FROM test_connection_admin@localhost;
GRANT CONNECTION_ADMIN ON *.* TO test_connection_admin@localhost;

--echo # in ssl_con
connection ssl_con;

--echo # Must pass
ALTER INSTANCE RELOAD TLS;

--echo # cleanup
--echo # in default connection
connection default;
disconnect ssl_con;
# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
DROP USER test_connection_admin@localhost;


--echo ################## FR 1.8 and 1.9: disable SSL on wrong values

--echo # Save the defaults
SET @orig_ssl_ca= @@global.ssl_ca;

--echo # Seet CA to invalid value
SET GLOBAL ssl_ca = 'gizmo';

--echo # Must fail and not change the SSL params
--error ER_DA_SSL_LIBRARY_ERROR
ALTER INSTANCE RELOAD TLS;

--echo # Must be 1
SELECT COUNT(*) FROM performance_schema.session_status
WHERE VARIABLE_NAME = 'Current_tls_ca' AND VARIABLE_VALUE = @orig_ssl_ca;

--echo # Must return gizmo
SELECT @@global.ssl_ca;

--echo # Must connect successfully
--exec $MYSQL --ssl-mode=required -e "SELECT 1"

--echo # Must pass with a warning and disable SSL
ALTER INSTANCE RELOAD TLS NO ROLLBACK ON ERROR;

--echo # Must be 1
SELECT COUNT(*) FROM performance_schema.session_status
WHERE VARIABLE_NAME = 'Current_tls_ca' AND VARIABLE_VALUE = 'gizmo';

--echo # Must fail to connect
--error 1
--exec $MYSQL --ssl-mode=required -e "SELECT 1"

--echo # cleanup
SET GLOBAL ssl_ca = @orig_ssl_ca;
ALTER INSTANCE RELOAD TLS;

--echo # FR 1.9: Must connect successfully
--exec $MYSQL --ssl-mode=required -e "SELECT 1"


--echo ################## FR2 and FR6: --ssl-* variables settable at runtime.
SET @orig_ssl_ca= @@global.ssl_ca;
SET @orig_ssl_cert= @@global.ssl_cert;
SET @orig_ssl_key= @@global.ssl_key;
SET @orig_ssl_capath= @@global.ssl_capath;
SET @orig_ssl_crl= @@global.ssl_crl;
SET @orig_ssl_crlpath= @@global.ssl_crlpath;
SET @orig_ssl_cipher= @@global.ssl_cipher;
SET @orig_tls_cipher= @@global.tls_ciphersuites;
SET @orig_tls_version= @@global.tls_version;

--echo # Must pass
SET GLOBAL ssl_ca = 'gizmo';
SET GLOBAL ssl_cert = 'gizmo';
SET GLOBAL ssl_key = 'gizmo';
SET GLOBAL ssl_capath = 'gizmo';
SET GLOBAL ssl_crl = 'gizmo';
SET GLOBAL ssl_crlpath = 'gizmo';

--echo # Must fail
--error ER_WRONG_VALUE_FOR_VAR
SET GLOBAL ssl_cipher = 'gizmo';
--error ER_WRONG_VALUE_FOR_VAR
SET GLOBAL tls_ciphersuites = 'gizmo';
--error ER_WRONG_VALUE_FOR_VAR
SET GLOBAL tls_version = 'gizmo';

--echo # Must fail
--error ER_GLOBAL_VARIABLE
SET SESSION ssl_ca = 'gizmo';
--error ER_GLOBAL_VARIABLE
SET SESSION ssl_cert = 'gizmo';
--error ER_GLOBAL_VARIABLE
SET SESSION ssl_key = 'gizmo';
--error ER_GLOBAL_VARIABLE
SET SESSION ssl_capath = 'gizmo';
--error ER_GLOBAL_VARIABLE
SET SESSION ssl_crl = 'gizmo';
--error ER_GLOBAL_VARIABLE
SET SESSION ssl_crlpath = 'gizmo';
--error ER_GLOBAL_VARIABLE
SET SESSION ssl_cipher = 'gizmo';
--error ER_GLOBAL_VARIABLE
SET SESSION tls_ciphersuites = 'gizmo';
--error ER_GLOBAL_VARIABLE
SET SESSION tls_version = 'gizmo';

--echo # FR6: Must return 9
SELECT VARIABLE_NAME FROM performance_schema.session_status WHERE
  VARIABLE_NAME IN
  ('Current_tls_ca', 'Current_tls_capath', 'Current_tls_cert',
   'Current_tls_key', 'Current_tls_version', 'Current_tls_cipher',
   'Current_tls_ciphersuites', 'Current_tls_crl', 'Current_tls_crlpath') AND
  VARIABLE_VALUE != 'gizmo'
  ORDER BY VARIABLE_NAME;

--echo # cleanup
SET GLOBAL ssl_ca = @orig_ssl_ca;
SET GLOBAL ssl_cert = @orig_ssl_cert;
SET GLOBAL ssl_key = @orig_ssl_key;
SET GLOBAL ssl_capath = @orig_ssl_capath;
SET GLOBAL ssl_crl = @orig_ssl_crl;
SET GLOBAL ssl_crlpath = @orig_ssl_crlpath;
SET GLOBAL ssl_cipher = @orig_ssl_cipher;
SET GLOBAL tls_ciphersuites = @orig_tls_ciphersuites;
SET GLOBAL tls_version = @orig_tls_version;


--echo ################## FR8: X plugin do not follow

--echo # Save the defaults
SET @orig_ssl_ca= @@global.ssl_ca;
SET @orig_ssl_cert= @@global.ssl_cert;
SET @orig_ssl_key= @@global.ssl_key;
SET @orig_mysqlx_ssl_ca= @@global.mysqlx_ssl_ca;
SET @orig_mysqlx_ssl_cert= @@global.mysqlx_ssl_cert;
SET @orig_mysqlx_ssl_key= @@global.mysqlx_ssl_key;

--echo # setting new values for ssl_cert, ssl_key and ssl_ca
--replace_result "$MYSQL_TEST_DIR" MYSQL_TEST_DIR
eval SET GLOBAL ssl_cert = "$MYSQL_TEST_DIR/std_data/server-cert-sha512.pem";
--replace_result "$MYSQL_TEST_DIR" MYSQL_TEST_DIR
eval SET GLOBAL ssl_key = "$MYSQL_TEST_DIR/std_data/server-key-sha512.pem";
--replace_result "$MYSQL_TEST_DIR" MYSQL_TEST_DIR
eval SET GLOBAL ssl_ca = "$MYSQL_TEST_DIR/std_data/ca-sha512.pem";
ALTER INSTANCE RELOAD TLS;

--echo # Check that X variables match the initial ones
--vertical_results
SELECT @@global.mysqlx_ssl_ca = @orig_mysqlx_ssl_ca,
       @@global.mysqlx_ssl_cert = @orig_mysqlx_ssl_cert,
       @@global.mysqlx_ssl_key = @orig_mysqlx_ssl_key;


--echo # cleanup
SET GLOBAL ssl_cert = @orig_ssl_cert;
SET GLOBAL ssl_key = @orig_ssl_key;
SET GLOBAL ssl_ca = @orig_ssl_ca;
ALTER INSTANCE RELOAD TLS;


--echo ################## End of dynamic SSL tests
