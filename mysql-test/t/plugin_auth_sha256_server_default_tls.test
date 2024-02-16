
set @orig_sql_mode_session= @@SESSION.sql_mode;
set @orig_sql_mode_global= @@GLOBAL.sql_mode;

CREATE USER 'kristofer';
ALTER USER 'kristofer' IDENTIFIED BY 'secret';
SELECT user, plugin FROM mysql.user ORDER BY user;
connect(con1,localhost,kristofer,secret,,,,SSL);
connection con1;
SELECT USER(),CURRENT_USER();
connection default;
disconnect con1;
DROP USER 'kristofer';

CREATE USER 'kristofer'@'localhost' IDENTIFIED WITH 'sha256_password';
CREATE USER 'kristofer2'@'localhost' IDENTIFIED WITH 'sha256_password';
ALTER USER 'kristofer'@'localhost' IDENTIFIED BY 'secret2';
ALTER USER 'kristofer2'@'localhost' IDENTIFIED BY 'secret2';
connect(con2,localhost,kristofer,secret2,,,,SSL);
connection con2;
SELECT USER(),CURRENT_USER();
SHOW GRANTS FOR 'kristofer'@'localhost';

--echo Change user (should succeed)
change_user kristofer2,secret2;
SELECT USER(),CURRENT_USER();

connection default;
disconnect con2;
--echo **** Client default_auth=sha_256_password and server default auth=sha256_password
# Why using sha256_password requires SSL?
# The current client library can't know if SSL was attempted or not
# when the default client auth is switched and hence it will only report
# that the connection is unencrypted forcing the client to ask for RSA keys.
# However, it is possible to inform the client library to enforce SSL
# and this will be enough for the
# sha256_password plugin to safely assume a secure connection despite it hasn't
# really been established yet.
--exec $MYSQL -ukristofer -psecret2 --default_auth=sha256_password --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem --ssl-mode=VERIFY_CA -e "select user(), current_user()"
--echo **** Client default_auth=native and server default auth=sha256_password
--exec $MYSQL -ukristofer -psecret2 --default_auth=mysql_native_password --ssl-mode=VERIFY_CA --ssl-ca=$MYSQL_TEST_DIR/std_data/cacert.pem --ssl-key=$MYSQL_TEST_DIR/std_data/client-key.pem --ssl-cert=$MYSQL_TEST_DIR/std_data/client-cert.pem -e "select user(), current_user()"

DROP USER 'kristofer'@'localhost';
DROP USER 'kristofer2'@'localhost';

CREATE USER 'kristofer'@'localhost';
ALTER USER 'kristofer'@'localhost' IDENTIFIED BY '';
connect(con3,localhost,kristofer,,,,,SSL);
connection con3;
SELECT USER(),CURRENT_USER();
SHOW GRANTS FOR 'kristofer'@'localhost';
connection default;
disconnect con3;
DROP USER 'kristofer'@'localhost';

CREATE USER 'kristofer'@'33.33.33.33';
ALTER USER 'kristofer'@'33.33.33.33' IDENTIFIED BY '';
--echo Connection should fail for localhost
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--disable_query_log
--error ER_ACCESS_DENIED_ERROR
connect(con4,127.0.0.1,kristofer,,,,,SSL);
--enable_query_log
DROP USER 'kristofer'@'33.33.33.33';

CREATE USER 'kristofer'@'localhost' IDENTIFIED BY 'awesomeness';
connect(con3,localhost,kristofer,awesomeness,,,,SSL);
connection con3;
SELECT USER(),CURRENT_USER();
SHOW GRANTS FOR 'kristofer'@'localhost';
connection default;
disconnect con3;
ALTER USER 'kristofer'@'localhost' IDENTIFIED BY '';
DROP USER 'kristofer'@'localhost';

set GLOBAL sql_mode= @orig_sql_mode_global;
set SESSION sql_mode= @orig_sql_mode_session;
