#
# Input(s) :
# $server_default_authentication_plugin
#

--echo # Restart the server with $server_default_authentication_plugin as default authnetication plugin
let $restart_file= $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
--exec echo "wait" > $restart_file
--shutdown_server
--source include/wait_until_disconnected.inc
--exec echo "restart:--default-authentication-plugin=$server_default_authentication_plugin" > $restart_file
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect


--echo # Create users

CREATE USER caching_sha2@localhost IDENTIFIED WITH 'caching_sha2_password' BY 'abcd';
CREATE USER native@localhost IDENTIFIED WITH 'mysql_native_password' BY 'abcd';
CREATE USER sha256@localhost IDENTIFIED WITH 'sha256_password' BY 'abcd';

--echo # Connection tests

--exec $MYSQL --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -ucaching_sha2 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --default-auth=mysql_native_password --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -ucaching_sha2 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --default-auth=sha256_password --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -ucaching_sha2 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
FLUSH PRIVILEGES;

--exec $MYSQL --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -ucaching_sha2 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
FLUSH PRIVILEGES;
--exec $MYSQL --default-auth=mysql_native_password --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -ucaching_sha2 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
FLUSH PRIVILEGES;
--exec $MYSQL --default-auth=sha256_password --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -ucaching_sha2 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
FLUSH PRIVILEGES;


--exec $MYSQL --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -unative -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --default-auth=mysql_native_password --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -unative -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --default-auth=sha256_password --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -unative -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
FLUSH PRIVILEGES;


--exec $MYSQL --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -usha256 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --default-auth=mysql_native_password --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -usha256 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --default-auth=sha256_password --protocol=TCP --host=127.0.0.1 -P$MASTER_MYPORT -usha256 -pabcd --ssl-mode=DISABLED --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT CURRENT_USER()" 2>&1
FLUSH PRIVILEGES;

--echo # Change user tests

--connect(caching_sha2_conn, localhost, caching_sha2, abcd,,,,SSL)
--change_user native, abcd
SELECT CURRENT_USER();
--change_user sha256, abcd
SELECT CURRENT_USER();
--change_user
SELECT CURRENT_USER();

--connect(native_conn, localhost, native, abcd,,,,SSL)
--change_user caching_sha2, abcd
SELECT CURRENT_USER();
--change_user sha256, abcd
SELECT CURRENT_USER();
--change_user
SELECT CURRENT_USER();

--connect(sha256_conn, localhost, sha256, abcd,,,,SSL)
--change_user caching_sha2, abcd
SELECT CURRENT_USER();
--change_user native, abcd
SELECT CURRENT_USER();
--change_user
SELECT CURRENT_USER();

--connection default
--disconnect caching_sha2_conn
--disconnect native_conn
--disconnect sha256_conn

--echo # Drop users

DROP USER caching_sha2@localhost;
DROP USER native@localhost;
DROP USER sha256@localhost;

--source include/force_restart.inc
