#
# Usage:
# --let $CIPHER_NAME=<cipher>
# --let $TLS_VERSION = "TLSv1.2" or "TLSv1.3"
# --let SEARCH_FILE=<file>
# --source search_cipher_blocked_error.inc
#

if ($TLS_VERSION == "TLSv1.2") {
  --let SEARCH_PATTERN=Value for option 'ssl_cipher' contains cipher '$CIPHER_NAME' that is blocked
  --source include/search_pattern.inc
  --let SEARCH_PATTERN=Value for option 'admin_ssl_cipher' contains cipher '$CIPHER_NAME' that is blocked
  --source include/search_pattern.inc
}

if ($TLS_VERSION == "TLSv1.3") {
  --let SEARCH_PATTERN=Value for option 'tls_ciphersuites' contains cipher '$CIPHER_NAME' that is blocked
  --source include/search_pattern.inc
  --let SEARCH_PATTERN=Value for option 'admin_tls_ciphersuites' contains cipher '$CIPHER_NAME' that is blocked
  --source include/search_pattern.inc
}
