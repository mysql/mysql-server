#
# Usage:
# --let $CIPHER_DB = dbname
# --let $USER = user
# --let $TLS_VERSION = "TLSv1.2" or "TLSv1.3"
#
# --source ../inc/ciphers_test.inc
#

--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err

--let $records = `SELECT JSON_LENGTH(ciphers) FROM $cipher_db.acceptable`
if ($records != "") {
  --echo
  --echo #-----------------------------------------------------------------------
  --echo
  --echo # Checking accetable ciphers
  --let $CIPHER_TABLE =acceptable
  --let $BLOCKED_CIPHERS=0
  --source set_ciphers.inc
  --let $i = 0
  while ($i < $records) {
    --let $CIPHER_NAME = `SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM $cipher_db.acceptable`
    if ($TLS_VERSION == "TLSv1.2") {
      --echo # Expecting connection success with cipher: $CIPHER_NAME on main channel
      --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER --ssl-cipher=$CIPHER_NAME -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
      --echo # Expecting connection success with cipher: $CIPHER_NAME on admin channel
      --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_ADMINPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER --ssl-cipher=$CIPHER_NAME -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    }

    if ($TLS_VERSION == "TLSv1.3") {
      --echo # Expecting connection success with cipher: $CIPHER_NAME on main channel
      --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.3 -u$USER --tls-ciphersuites=$CIPHER_NAME -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
      --echo # Expecting connection success with cipher: $CIPHER_NAME on admin channel
      --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_ADMINPORT --ssl-mode=REQUIRED --tls-version=TLSv1.3 -u$USER --tls-ciphersuites=$CIPHER_NAME -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    }
    --inc $i
  }
  --echo
  --echo #-----------------------------------------------------------------------
  --echo
}

--let $records = `SELECT JSON_LENGTH(ciphers) FROM $cipher_db.blocked`
if ($records != "") {
  --echo
  --echo #-----------------------------------------------------------------------
  --echo
  --echo # Checking blocked ciphers
  --let $CIPHER_TABLE =blocked
  --let $BLOCKED_CIPHERS=1
  --source set_ciphers.inc
  --let $i = 0
  while ($i < $records) {
    --let $CIPHER_NAME = `SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM $cipher_db.blocked`
    --source search_cipher_blocked_error.inc
    if ($TLS_VERSION == "TLSv1.2") {
      --echo # Expecting connection failure wiith cipher: $CIPHER_NAME on main channel
      --replace_regex /SSL connection error:.*/SSL connection error: Failed to set ciphers to use/
      --error 1
      --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER --ssl-cipher=$CIPHER_NAME -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
      --echo # Expecting connection failure wiith cipher: $CIPHER_NAME on admin channel
      --replace_regex /SSL connection error:.*/SSL connection error: Failed to set ciphers to use/
      --error 1
      --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_ADMINPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER --ssl-cipher=$CIPHER_NAME -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    }

    if ($TLS_VERSION == "TLSv1.3") {
      --echo # Expecting connection failure wiith cipher: $CIPHER_NAME on main channel
      --replace_regex /SSL connection error:.*/SSL connection error: Failed to set ciphers to use/
      --error 1
      --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.3 -u$USER --tls-ciphersuites=$CIPHER_NAME -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
      --echo # Expecting connection failure wiith cipher: $CIPHER_NAME on admin channel
      --replace_regex /SSL connection error:.*/SSL connection error: Failed to set ciphers to use/
      --error 1
      --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_ADMINPORT --ssl-mode=REQUIRED --tls-version=TLSv1.3 -u$USER --tls-ciphersuites=$CIPHER_NAME -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    }
    --inc $i
  }
  --echo
  --echo #-----------------------------------------------------------------------
  --echo
}
