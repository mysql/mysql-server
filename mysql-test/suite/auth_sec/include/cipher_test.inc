#
# Usage:
# --let $CIPHER_DB = dbname
# --let $USER = user
# --let $PASSWORD = password
#
# --source ../inc/cipher_test.inc
#

--let $records = `SELECT JSON_LENGTH(ciphers) FROM $cipher_db.p`
if ($records != "") {
  --let $i = 0
  while ($i < $records) {
    --let $cipher_name = `SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM $cipher_db.p`
    --echo # Expecting connection success with cipher: $cipher_name
    --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER -p$PASSWORD --ssl-cipher=$cipher_name -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    --inc $i
  }
}

--let $records = `SELECT JSON_LENGTH(ciphers) FROM $cipher_db.a`
if ($records != "") {
  --let $i = 0
  while ($i < $records) {
    --let $cipher_name = `SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM $cipher_db.a`
    --echo # Expecting connection success with cipher: $cipher_name
    --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER -p$PASSWORD --ssl-cipher=$cipher_name -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    --inc $i
  }
}

--let $records = `SELECT JSON_LENGTH(ciphers) FROM $cipher_db.d`
if ($records != "") {
  --let $i = 0
  while ($i < $records) {
    --let $cipher_name = `SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM $cipher_db.d`
    --echo # Expecting connection success with cipher: $cipher_name
    --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER -p$PASSWORD --ssl-cipher=$cipher_name -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    --inc $i
  }
}

--let $records = `SELECT JSON_LENGTH(ciphers) FROM $cipher_db.n`
if ($records != "") {
  --let $i = 0
  while ($i < $records) {
    --let $cipher_name = `SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM $cipher_db.n`
    --eval SET @@global.ssl_cipher = '$cipher_name'
    --eval ALTER INSTANCE RELOAD TLS
    --echo # Expecting connection success with cipher: $cipher_name
    --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER -p$PASSWORD --ssl-cipher=$cipher_name -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    --inc $i
  }
  --eval SET @@global.ssl_cipher = NULL
  --eval ALTER INSTANCE RELOAD TLS
}

--let $records = `SELECT JSON_LENGTH(ciphers) FROM $cipher_db.b`
if ($records != "") {
  --let $i = 0
  while ($i < $records) {
    --let $cipher_name = `SELECT JSON_UNQUOTE(JSON_EXTRACT(ciphers, '$[$i]')) FROM $cipher_db.b`
    --echo # Expecting connection failure wiith cipher: $cipher_name
    --replace_regex /SSL connection error:.*/SSL connection error/
    --error 1
    --exec $MYSQL --protocol=TCP --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED --tls-version=TLSv1.2 -u$USER -p$PASSWORD --ssl-cipher=$cipher_name -e "SHOW STATUS LIKE 'Ssl_cipher'" 2>&1
    --inc $i
  }
}
