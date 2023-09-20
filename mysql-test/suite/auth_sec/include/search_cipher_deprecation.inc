#
# Usage:
# --let $CIPHER_NAME=<cipher>
# --let $TLS_VERSION = "TLSv1.2" or "TLSv1.3"
# --source search_cipher_deprecation.inc
#

--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err

if ($TLS_VERSION == "TLSv1.2") {
  --let SEARCH_PATTERN=Value for option 'ssl_cipher' contains cipher '$CIPHER_NAME' that is either blocked or deprecated
  --source include/search_pattern.inc
  --let SEARCH_PATTERN=Value for option 'admin_ssl_cipher' contains cipher '$CIPHER_NAME' that is either blocked or deprecated
  --source include/search_pattern.inc
}

if ($TLS_VERSION == "TLSv1.3") {
  --let SEARCH_PATTERN=Value for option 'tls_ciphersuites' contains cipher '$CIPHER_NAME' that is either blocked or deprecated
  --source include/search_pattern.inc
  --let SEARCH_PATTERN=Value for option 'admin_tls_ciphersuites' contains cipher '$CIPHER_NAME' that is either blocked or deprecated
  --source include/search_pattern.inc
}
