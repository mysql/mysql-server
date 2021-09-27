#-----------------------------------------------------------------------

# Setup

CREATE USER arthurdent@localhost IDENTIFIED BY 'abcd';
CREATE DATABASE cipher_data;
CREATE TABLE cipher_data.p(ciphers JSON);
CREATE TABLE cipher_data.a(ciphers JSON);
CREATE TABLE cipher_data.d(ciphers JSON);
CREATE TABLE cipher_data.n(ciphers JSON);
CREATE TABLE cipher_data.b(ciphers JSON);

#-----------------------------------------------------------------------

# Ciphers: ECDHE-RSA | Cert/Key type: RSA

SET @@global.ssl_ca='MYSQL_TEST_DIR/std_data/cacert.pem';
SET @@global.ssl_cert='MYSQL_TEST_DIR/std_data/server-cert.pem';
SET @@global.ssl_key='MYSQL_TEST_DIR/std_data/server-key.pem';
ALTER INSTANCE RELOAD TLS;
TRUNCATE TABLE cipher_data.p;
TRUNCATE TABLE cipher_data.a;
TRUNCATE TABLE cipher_data.d;
TRUNCATE TABLE cipher_data.n;
TRUNCATE TABLE cipher_data.b;
INSERT INTO cipher_data.p VALUES ('["ECDHE-RSA-AES128-GCM-SHA256",
                                    "ECDHE-RSA-AES128-SHA256"]');
INSERT INTO cipher_data.a VALUES ('["ECDHE-RSA-AES256-GCM-SHA384",
                                    "ECDHE-RSA-AES256-SHA384"]');
INSERT INTO cipher_data.d VALUES ('["ECDHE-RSA-AES128-SHA",
                                    "ECDHE-RSA-AES256-SHA"]');
INSERT INTO cipher_data.b VALUES ('["ECDHE-RSA-DES-CBC3-SHA",
                                    "ECDHE-RSA-RC4-SHA"]');
# Expecting connection success with cipher: ECDHE-RSA-AES128-GCM-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-RSA-AES128-GCM-SHA256
# Expecting connection success with cipher: ECDHE-RSA-AES128-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-RSA-AES128-SHA256
# Expecting connection success with cipher: ECDHE-RSA-AES256-GCM-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-RSA-AES256-GCM-SHA384
# Expecting connection success with cipher: ECDHE-RSA-AES256-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-RSA-AES256-SHA384
# Expecting connection success with cipher: ECDHE-RSA-AES128-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-RSA-AES128-SHA
# Expecting connection success with cipher: ECDHE-RSA-AES256-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-RSA-AES256-SHA
# Expecting connection failure wiith cipher: ECDHE-RSA-DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: ECDHE-RSA-RC4-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error

#-----------------------------------------------------------------------

# Ciphers: DHE-RSA | Cert/Key type: RSA

TRUNCATE TABLE cipher_data.p;
TRUNCATE TABLE cipher_data.a;
TRUNCATE TABLE cipher_data.d;
TRUNCATE TABLE cipher_data.n;
TRUNCATE TABLE cipher_data.b;
INSERT INTO cipher_data.a VALUES ('["DHE-RSA-AES256-GCM-SHA384",
                                    "DHE-RSA-AES128-GCM-SHA256",
                                    "DHE-RSA-AES256-SHA256",
                                    "DHE-RSA-AES128-SHA256"]');
INSERT INTO cipher_data.d VALUES ('["DHE-RSA-AES256-SHA",
                                    "DHE-RSA-AES128-SHA"]');
INSERT INTO cipher_data.n VALUES ('["DHE-RSA-CAMELLIA256-SHA",
                                    "DHE-RSA-CAMELLIA128-SHA"]');
INSERT INTO cipher_data.b VALUES ('["DHE-RSA-DES-CBC3-SHA"]');
# Expecting connection success with cipher: DHE-RSA-AES256-GCM-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-GCM-SHA384
# Expecting connection success with cipher: DHE-RSA-AES128-GCM-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	DHE-RSA-AES128-GCM-SHA256
# Expecting connection success with cipher: DHE-RSA-AES256-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-SHA256
# Expecting connection success with cipher: DHE-RSA-AES128-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	DHE-RSA-AES128-SHA256
# Expecting connection success with cipher: DHE-RSA-AES256-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	DHE-RSA-AES256-SHA
# Expecting connection success with cipher: DHE-RSA-AES128-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	DHE-RSA-AES128-SHA
SET @@global.ssl_cipher = 'DHE-RSA-CAMELLIA256-SHA';
ALTER INSTANCE RELOAD TLS;
# Expecting connection success with cipher: DHE-RSA-CAMELLIA256-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	DHE-RSA-CAMELLIA256-SHA
SET @@global.ssl_cipher = 'DHE-RSA-CAMELLIA128-SHA';
ALTER INSTANCE RELOAD TLS;
# Expecting connection success with cipher: DHE-RSA-CAMELLIA128-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	DHE-RSA-CAMELLIA128-SHA
SET @@global.ssl_cipher = NULL;
ALTER INSTANCE RELOAD TLS;
# Expecting connection failure wiith cipher: DHE-RSA-DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error

#-----------------------------------------------------------------------

# Ciphers: other | Cert/Key type: RSA

TRUNCATE TABLE cipher_data.p;
TRUNCATE TABLE cipher_data.a;
TRUNCATE TABLE cipher_data.d;
TRUNCATE TABLE cipher_data.n;
TRUNCATE TABLE cipher_data.b;
INSERT INTO cipher_data.b VALUES ('["DES-CBC3-SHA", "RC4-SHA",
                                    "RC4-MD5", "PSK-RC4-SHA",
                                    "PSK-AES256-CBC-SHA", "PSK-AES128-CBC-SHA",
                                    "PSK-3DES-EDE-CBC-SHA",
                                    "EDH-RSA-DES-CBC3-SHA",
                                    "EDH-DSS-DES-CBC3-SHA", "DES-CBC3-SHA"]');
# Expecting connection failure wiith cipher: DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: RC4-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: RC4-MD5
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: PSK-RC4-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: PSK-AES256-CBC-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: PSK-AES128-CBC-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: PSK-3DES-EDE-CBC-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: EDH-RSA-DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: EDH-DSS-DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error

#-----------------------------------------------------------------------

# Ciphers: ECDHE-ECDSA | Cert/Key type: ECDH certificate signed with RSA

SET @@global.ssl_ca='MYSQL_TEST_DIR/std_data/ecdh_rsa_certs/cacert.pem';
SET @@global.ssl_cert='MYSQL_TEST_DIR/std_data/ecdh_rsa_certs/server-cert.pem';
SET @@global.ssl_key='MYSQL_TEST_DIR/std_data/ecdh_rsa_certs/server-key.pem';
ALTER INSTANCE RELOAD TLS;
TRUNCATE TABLE cipher_data.p;
TRUNCATE TABLE cipher_data.a;
TRUNCATE TABLE cipher_data.d;
TRUNCATE TABLE cipher_data.n;
TRUNCATE TABLE cipher_data.b;
INSERT INTO cipher_data.p VALUES ('["ECDHE-ECDSA-AES256-GCM-SHA384",
                                    "ECDHE-ECDSA-AES128-GCM-SHA256",
                                    "ECDHE-ECDSA-AES128-SHA256"]');
INSERT INTO cipher_data.a VALUES ('["ECDHE-ECDSA-AES256-SHA384"]');
INSERT INTO cipher_data.d VALUES ('["ECDHE-ECDSA-AES256-SHA",
                                    "ECDHE-ECDSA-AES128-SHA"]');
INSERT INTO cipher_data.b VALUES ('["ECDHE-ECDSA-DES-CBC3-SHA",
                                    "ECDHE-ECDSA-RC4-SHA"]');
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-GCM-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-GCM-SHA384
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-GCM-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-GCM-SHA256
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-SHA256
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-SHA384
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-SHA
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-SHA
# Expecting connection failure wiith cipher: ECDHE-ECDSA-DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: ECDHE-ECDSA-RC4-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error

#-----------------------------------------------------------------------

# Ciphers: ECDHE-ECDSA | Cert/Key type: ECDH certificate signed with DSA

SET @@global.ssl_ca='MYSQL_TEST_DIR/std_data/ecdh_dsa_certs/cacert.pem';
SET @@global.ssl_cert='MYSQL_TEST_DIR/std_data/ecdh_dsa_certs/server-cert.pem';
SET @@global.ssl_key='MYSQL_TEST_DIR/std_data/ecdh_dsa_certs/server-key.pem';
ALTER INSTANCE RELOAD TLS;
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-GCM-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-GCM-SHA384
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-GCM-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-GCM-SHA256
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-SHA256
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-SHA384
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-SHA
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-SHA
# Expecting connection failure wiith cipher: ECDHE-ECDSA-DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: ECDHE-ECDSA-RC4-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error

#-----------------------------------------------------------------------

# Ciphers: ECDHE-ECDSA | Cert/Key type: ECDSA

SET @@global.ssl_ca='MYSQL_TEST_DIR/std_data/ecdsa_certs/cacert.pem';
SET @@global.ssl_cert='MYSQL_TEST_DIR/std_data/ecdsa_certs/server-cert.pem';
SET @@global.ssl_key='MYSQL_TEST_DIR/std_data/ecdsa_certs/server-key.pem';
ALTER INSTANCE RELOAD TLS;
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-GCM-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-GCM-SHA384
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-GCM-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-GCM-SHA256
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-SHA256
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-SHA256
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-SHA384
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-SHA384
# Expecting connection success with cipher: ECDHE-ECDSA-AES256-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES256-SHA
# Expecting connection success with cipher: ECDHE-ECDSA-AES128-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
Variable_name	Value
Ssl_cipher	ECDHE-ECDSA-AES128-SHA
# Expecting connection failure wiith cipher: ECDHE-ECDSA-DES-CBC3-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error
# Expecting connection failure wiith cipher: ECDHE-ECDSA-RC4-SHA
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2026 (HY000): SSL connection error

#-----------------------------------------------------------------------

# Cleanup

DROP DATABASE cipher_data;
DROP USER arthurdent@localhost;

#-----------------------------------------------------------------------
