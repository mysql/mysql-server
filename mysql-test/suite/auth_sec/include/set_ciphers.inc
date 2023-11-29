#
# Usage:
# --let $CIPHER_DB = <db>
# --let $CIPHER_TABLE = <table>
# --let $TLS_VERSION = "TLSv1.2" or "TLSv1.3"
# --let $BLOCKED_CIPHERS = 0 or 1
# --let #OPENSSL_102 = 0 or 1
#
# --source ../inc/set_ciphers.inc
#

--let $records = `SELECT JSON_LENGTH(ciphers) FROM $cipher_db.$cipher_table`
if ($records != "") {
  --let $i = 0
  --disable_query_log
  --eval SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM $cipher_db.$cipher_table INTO @ciphers
  --inc $i
  while ($i < $records) {
    --eval SELECT CONCAT(@ciphers, ':', JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]'))) FROM $cipher_db.$cipher_table INTO @ciphers
    --inc $i
  }

  --let $CIPHERS = `SELECT @ciphers`
  --echo # Setting server ciphers: $CIPHERS
  SELECT @@global.ssl_cipher INTO @saved_ciphers;
  SELECT @@global.tls_ciphersuites INTO @saved_ciphersuites;
  SELECT @@global.admin_ssl_cipher INTO @saved_admin_ciphers;
  SELECT @@global.admin_tls_ciphersuites INTO @saved_admin_ciphersuites;

  if ($TLS_VERSION == "TLSv1.2") {
    if ($BLOCKED_CIPHERS == 0) {
      SET GLOBAL ssl_cipher=@ciphers;
      SET GLOBAL admin_ssl_cipher=@ciphers;
    }

    if ($BLOCKED_CIPHERS == 1) {
      --error ER_WRONG_VALUE_FOR_VAR
      SET GLOBAL ssl_cipher=@ciphers;
      --error ER_WRONG_VALUE_FOR_VAR
      SET GLOBAL admin_ssl_cipher=@ciphers;
    }
  }

  if ($TLS_VERSION == "TLSv1.3") {
    if ($BLOCKED_CIPHERS == 0) {
      SET GLOBAL tls_ciphersuites=@ciphers;
      SET GLOBAL admin_tls_ciphersuites=@ciphers;
    }

    if ($BLOCKED_CIPHERS == 1) {
      --error ER_WRONG_VALUE_FOR_VAR
      SET GLOBAL tls_ciphersuites=@ciphers;
      --error ER_WRONG_VALUE_FOR_VAR
      SET GLOBAL admin_tls_ciphersuites=@ciphers;
    }
  }

  if ($BLOCKED_CIPHERS == 0) {
    ALTER INSTANCE RELOAD TLS FOR CHANNEL mysql_main;
    ALTER INSTANCE RELOAD TLS FOR CHANNEL mysql_admin;
  }
  --enable_query_log
}
