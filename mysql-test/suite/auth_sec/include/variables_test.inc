#
# Usage:
# --let $TLS_VERSION = TLSv1.2 or TLSv1.3
# --source ../inc/variables_test.inc
#

--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err

--let $records = `SELECT JSON_LENGTH(ciphers) FROM cipher_db.ciphers_data`
if ($records != "") {
  --echo
  --echo #-----------------------------------------------------------------------
  --echo
  --let $i = 0
  --disable_query_log
  --eval SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM cipher_db.ciphers_data INTO @ciphers
  --inc $i
  while ($i < $records) {
    --eval SELECT CONCAT(@ciphers, ':', JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]'))) FROM cipher_db.ciphers_data INTO @ciphers
    --inc $i
  }
  --let $CIPHERS = `SELECT @ciphers`
  --echo # Setting server ciphers: $CIPHERS
  if ($TLS_VERSION == "TLSv1.2") {
    --error 0, ER_WRONG_VALUE_FOR_VAR
    --eval SET GLOBAL ssl_cipher='$CIPHERS'
    --error 0, ER_WRONG_VALUE_FOR_VAR
    --eval SET GLOBAL admin_ssl_cipher='$CIPHERS'
  }

  if ($TLS_VERSION == "TLSv1.3") {
    --error 0, ER_WRONG_VALUE_FOR_VAR
    --eval SET GLOBAL tls_ciphersuites='$CIPHERS'
    --error 0, ER_WRONG_VALUE_FOR_VAR
    --eval SET GLOBAL admin_tls_ciphersuites='$CIPHERS'
  }
  --let $i = 0
  while ($i < $records) {
    --echo
    --let $CIPHER_NAME = `SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM cipher_db.ciphers_data`
    --echo # Checking error messages (if any) for $CIPHER_NAME
    --source search_cipher_blocked_error.inc
    --echo
    --inc $i
  }
  --echo
  --echo #-----------------------------------------------------------------------
  --echo
}
