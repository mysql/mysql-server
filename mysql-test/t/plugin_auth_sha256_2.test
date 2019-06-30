--source include/mysql_upgrade_preparation.inc

CREATE USER 'kristofer' IDENTIFIED BY 'secret';
SELECT user FROM mysql.user ORDER BY user;
--exec $MYSQL -ukristofer -psecret --default_auth=sha256_password -e "select user(), current_user()"
--exec $MYSQL -ukristofer -psecret --default_auth=mysql_native_password -e "select user(), current_user()"
--exec $MYSQL -ukristofer -psecret --default_auth=sha256_password --server_public_key_path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "select user(), current_user()"
DROP USER 'kristofer';

CREATE USER 'kristofer'@'localhost' IDENTIFIED BY 'secret2';
--exec $MYSQL -ukristofer -psecret2 --default_auth=sha256_password -e "select user(), current_user()"
--exec $MYSQL -ukristofer -psecret2 --default_auth=mysql_native_password -e "select user(), current_user()"
--exec $MYSQL -ukristofer -psecret2 --default_auth=sha256_password --server_public_key_path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "select user(), current_user()"
SHOW GRANTS FOR 'kristofer'@'localhost';
DROP USER 'kristofer'@'localhost';

CREATE USER 'kristofer'@'localhost' IDENTIFIED BY '123';
--exec $MYSQL -ukristofer -p123 --default_auth=sha256_password -e "select user(), current_user()"
--exec $MYSQL -ukristofer -p123 --default_auth=mysql_native_password -e "select user(), current_user()"
--exec $MYSQL -ukristofer -p123 --default_auth=sha256_password --server_public_key_path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "select user(), current_user()"
SHOW GRANTS FOR 'kristofer'@'localhost';
DROP USER 'kristofer'@'localhost';

CREATE USER 'kristofer'@'33.33.33.33' IDENTIFIED BY '123';
--echo Connection should fail for localhost
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--disable_query_log
--error ER_ACCESS_DENIED_ERROR
connect(con4,127.0.0.1,kristofer,,,);
--enable_query_log
DROP USER 'kristofer'@'33.33.33.33';


