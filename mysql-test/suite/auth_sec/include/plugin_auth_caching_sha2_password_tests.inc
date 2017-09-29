
# Create users

CREATE USER arthurdent@localhost IDENTIFIED WITH caching_sha2_password BY 'abcd';
CREATE USER marvintheparanoidandroid@localhost IDENTIFIED WITH caching_sha2_password BY 'efghi';
CREATE USER zaphodbeeblebrox@localhost IDENTIFIED WITH caching_sha2_password BY 'xyz';

# Connection using RSA keys - cache entry will be created for arthudent.
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1

# Now even plaintext connection should work
--connect(conn_arthurdent, localhost, arthurdent, abcd,,,,)
SELECT USER(), CURRENT_USER();

# Let's try one more time
--connect(conn_arthurdent_2, localhost, arthurdent, abcd,,,,)
SELECT USER(), CURRENT_USER();

# Change the password with SET PASSWORD
SET PASSWORD = 'dcba';

# Old password based scramble shouldn't work
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error ER_ACCESS_DENIED_ERROR
--connect(conn_arthurdent_3, localhost, arthurdent, abcd,,,,SSL)

# Neither should the full authentication - Actual error ER_ACCESS_DENIED_ERROR
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error ER_ACCESS_DENIED_ERROR
--connect(conn_arthurdent_3, localhost, arthurdent, abcd,,,,SSL)
--error 1
--exec $MYSQL -uarthurdent -pabcd --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1

# Scrmable based on new password is not yet cached on server side - Actual error CR_AUTH_PLUGIN_ERR
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pdcba --ssl-mode=DISABLED -e "SELECT USER(), CURRENT_USER()" 2>&1

# But if you perform full authentication, then everything should be set
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pdcba --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1

# Try with plaintext connection, it should work now
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pdcba -e "SELECT USER(), CURRENT_USER()" 2>&1

# Now try password change through ALTER USER
--exec $MYSQL -uarthurdent -pdcba -e "ALTER USER arthurdent@localhost IDENTIFIED BY 'acbd';" 2>&1

# Same story : Old scramble - no, old password - no, new scramble - not yet available, new password - yes, then new scramble -yes
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error ER_ACCESS_DENIED_ERROR
--connect(conn_arthurdent_4, localhost, arthurdent, dcba,,,,SSL)
--error ER_ACCESS_DENIED_ERROR
--connect(conn_arthurdent_3, localhost, arthurdent, dcba,,,,SSL)
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pdcba --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pacbd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL -uarthurdent --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -pacbd --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pacbd -e "SELECT USER(), CURRENT_USER()" 2>&1

# Same should happen if other changes the password
--connection default
SET PASSWORD FOR arthurdent@localhost = 'dbca';

# Same story : Old scramble - no, old password - no, new scramble - not yet available, new password - yes, then new scramble -yes
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error ER_ACCESS_DENIED_ERROR
--connect(conn_arthurdent_4, localhost, arthurdent, acbd,,,,)
--error ER_ACCESS_DENIED_ERROR
--connect(conn_arthurdent_3, localhost, arthurdent, acbd,,,,SSL)
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pacbd --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pdbca -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pdbca --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pdbca -e "SELECT USER(), CURRENT_USER()" 2>&1

--connection default
ALTER USER arthurdent@localhost IDENTIFIED BY 'abcd';

# Same story : Old scramble - no, old password - no, new scramble - not yet available, new password - yes, then new scramble -yes
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error ER_ACCESS_DENIED_ERROR
--connect(conn_arthurdent_4, localhost, arthurdent, dbca,,,,)
--error ER_ACCESS_DENIED_ERROR
--connect(conn_arthurdent_3, localhost, arthurdent, dbca,,,,SSL)
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pdbca --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1

# FLUSH PRIVILEGES should clear the sha2 cache as well
--connection default
FLUSH PRIVILEGES;

--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIREd -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--connect(conn_arthurdent_6, localhost, arthurdent, abcd,,,,)
SELECT USER(), CURRENT_USER();
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1

# caching_sha2_password as client's default authentication
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd --default-auth=caching_sha2_password -e "SELECT USER(), CURRENT_USER()" 2>&1

# Expire password - Should force full authentication
--connection default
ALTER USER arthurdent@localhost PASSWORD EXPIRE;
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED -uarthurdent -pabcd --connect-expired-password -e "SET PASSWORD='abcd';" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=REQUIRED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1

# RENAME USER should clear the sha2 cache as well
--connection default
RENAME USER arthurdent@localhost TO slartibartfast@localhost;

--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uslartibartfast -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uslartibartfast -pabcd --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1

# Rename again
--connection default
RENAME USER slartibartfast@localhost TO arthurdent@localhost;
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd --server-public-key-path=$MYSQL_TEST_DIR/std_data/rsa_public_key.pem -e "SELECT USER(), CURRENT_USER()" 2>&1

# SSL connection - cache entry will be created for marvintheparanoidandroid
--connect(conn_marvintheparanoidandroid,localhost,marvintheparanoidandroid,efghi,,,,SSL)
SELECT USER(), CURRENT_USER();

# Now even plaintext connection should work
--connect(conn_marvintheparanoidandroid_2,localhost,marvintheparanoidandroid,efghi,,,,)
SELECT USER(), CURRENT_USER();

# Let's try one more time
--connect(conn_marvintheparanoidandroid_3,localhost,marvintheparanoidandroid,efghi,,,,)
SELECT USER(), CURRENT_USER();

# Try change_user
change_user marvintheparanoidandroid, efghi;
SELECT USER(), CURRENT_USER();

# Plaintext connection - cache entry will be created for zaphodbeeblebrox
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -uzaphodbeeblebrox -pxyz --get-server-public-key -e "SELECT USER(), CURRENT_USER()" 2>&1

# Now even plaintext connection should work
--connect(conn_zaphodbeeblebrox, localhost, zaphodbeeblebrox, xyz,,,,)
SELECT USER(), CURRENT_USER();

# Let's try one more time
--connect(conn_zaphodbeeblebrox_2, localhost, zaphodbeeblebrox, xyz,,,,)
SELECT USER(), CURRENT_USER();

# SHOW CREATE USER
--connection default
--replace_regex /AS .*$/AS '<non-deterministic-password-hash>'/
SHOW CREATE USER arthurdent@localhost;
--replace_regex /AS .*$/AS '<non-deterministic-password-hash>'/
SHOW CREATE USER marvintheparanoidandroid@localhost;
--replace_regex /AS .*$/AS '<non-deterministic-password-hash>'/
SHOW CREATE USER zaphodbeeblebrox@localhost;


# Cleanup - Close connections and remove users

--connection default
--disconnect conn_arthurdent
--disconnect conn_arthurdent_2
--disconnect conn_arthurdent_6
--disconnect conn_marvintheparanoidandroid
--disconnect conn_marvintheparanoidandroid_2
--disconnect conn_marvintheparanoidandroid_3
--disconnect conn_zaphodbeeblebrox
--disconnect conn_zaphodbeeblebrox_2

DROP USER arthurdent@localhost;
DROP USER marvintheparanoidandroid@localhost;
DROP USER zaphodbeeblebrox@localhost;

