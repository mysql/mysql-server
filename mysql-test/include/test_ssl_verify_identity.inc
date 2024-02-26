## Script test_ssl_verify_identity.inc
#
#  === Purpose ===
#    This test verifies that while verifying the server certificates
#    when ssl-mode=VERIFY_IDENTITY, the DNS/IPs provided in the Subject
#    Alternative Names (which can be provided as an extension in X509)
#    fields are also checked for apart from the Common Name in the subject.
#    Applicable for openssl versions 1.0.2 and greater.
#
#  $param_test_exe -
#    defines the application which should run the test,
#    the application must be $MYSQL command line compatible
#
#
#  $param_cipher_variable -
#    defines the name of variable which values is going to be
#    checked for SSL cipher used be current test connection
#
#  $param_verify_identity_error -
#    set expected error in case of invalid hostname
#
## Usage
#
# --let $param_test_exe = $MYSQL|$MYSQLXTEST
# --let $param_cipher_variable = Ssl_cipher
# --let $param_verify_identity_error = some string...
# --source include/test_ssl_verify_identity.inc

#
# === Related bugs and/or worklogs ===
# Bug #16211011 - SSL CERTIFICATE SUBJECT ALT NAMES WITH IPS NOT RESPECTED WITH ssl-mode=VERIFY_IDENTITY
# Bug #29691694 - LIBMYSQLXCLIENT --SSL-MODE=VERIFY_IDENTITY DOESN'T USE SUBJECT ALT NAMES
#
# Note that these test cases are written keeping in mind that the openssl version used by the system will
# be 1.0.2+. For older versions of openssl, the test will be skipped.

--source include/check_openssl_version.inc

--echo ### Trying to connect with ssl-mode as DISABLED. This should establish an unencrypted connection.
exec $param_test_exe
  --ssl-mode=DISABLED
  --ssl-ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify-san.pem
  --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert-verify-san.pem
  --ssl-key=$MYSQL_TEST_DIR/std_data/client-key-verify-san.pem
  -e "SHOW STATUS LIKE '$param_cipher_variable'" test 2> $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;
cat_file $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;

--echo ### Trying to connect with ssl-mode as REQUIRED. This should establish an encrypted connection.
--replace_regex $ALLOWED_CIPHERS_REGEX
exec $param_test_exe
  --ssl-mode=REQUIRED
  --ssl-ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify-san.pem
  --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert-verify-san.pem
  --ssl-key=$MYSQL_TEST_DIR/std_data/client-key-verify-san.pem
  -e "SHOW STATUS LIKE '$param_cipher_variable'" test 2> $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;
cat_file $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;

--echo ### Trying to connect with ssl-mode as VERIFY_CA. This should establish an encrypted connection.
--replace_regex $ALLOWED_CIPHERS_REGEX
exec $param_test_exe
  --ssl-mode=VERIFY_CA
  --ssl-ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify-san.pem
  --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert-verify-san.pem
  --ssl-key=$MYSQL_TEST_DIR/std_data/client-key-verify-san.pem
  -e "SHOW STATUS LIKE '$param_cipher_variable'" test 2> $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;
cat_file $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;

--echo ### Trying to connect with ssl-mode as VERIFY_IDENTITY. This should establish an encrypted connection.
--replace_regex $ALLOWED_CIPHERS_REGEX
exec $param_test_exe
  --ssl-mode=VERIFY_IDENTITY
  --ssl-ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify-san.pem
  --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert-verify-san.pem
  --ssl-key=$MYSQL_TEST_DIR/std_data/client-key-verify-san.pem
  -e "SHOW STATUS LIKE '$param_cipher_variable'" test 2> $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;
cat_file $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;

--echo ### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as nonexistent. This should fail.
--error 1
exec $param_test_exe
  --host=nonexistent
  --ssl-mode=VERIFY_IDENTITY
  --ssl-ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify-san.pem
  --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert-verify-san.pem
  --ssl-key=$MYSQL_TEST_DIR/std_data/client-key-verify-san.pem
  -e "SHOW STATUS LIKE '$param_cipher_variable'" test 2> $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;
let SEARCH_FILE= $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;

--echo #Search for the error in the file
let SEARCH_PATTERN=$param_verify_identity_error;
source include/search_pattern.inc;

--echo ### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as localhost. This should establish an encrypted connection as localhost is present in Alternative Subject Name in the certificate.
--replace_regex $ALLOWED_CIPHERS_REGEX
exec $param_test_exe
  --host=localhost
  --ssl-mode=VERIFY_IDENTITY
  --ssl-ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify-san.pem
  --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert-verify-san.pem
  --ssl-key=$MYSQL_TEST_DIR/std_data/client-key-verify-san.pem
  -e "SHOW STATUS LIKE '$param_cipher_variable'" test 2> $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;
cat_file $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;

--echo ### Trying to connect with ssl-mode as VERIFY_IDENTITY and hostname as 127.0.0.1. This should establish an encrypted connection as localhost is present in Alternative Subject Name in the certificate.
--replace_regex $ALLOWED_CIPHERS_REGEX
--exec $param_test_exe--host=127.0.0.1 --ssl-mode=VERIFY_IDENTITY --ssl-ca=$MYSQL_TEST_DIR/std_data/ca-cert-verify-san.pem  --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert-verify-san.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key-verify-san.pem -e "SHOW STATUS LIKE '$param_cipher_variable'" test 2> $MYSQLTEST_VARDIR/tmp/bug24732452_stderr
cat_file $MYSQLTEST_VARDIR/tmp/bug24732452_stderr;

#Cleanup
--remove_file $MYSQLTEST_VARDIR/tmp/bug24732452_stderr
