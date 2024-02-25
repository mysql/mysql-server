# Parameters:
# $auth_plugin - Authentication plugin
# $ssl_option - To supply extra ssl argument

--echo #-----------------------------------------------------------------------
--echo # Setup : Create users

--connection default
# Create test users
--eval CREATE USER arthurdent@localhost IDENTIFIED WITH '$auth_plugin' BY 'abcd'

--echo #-----------------------------------------------------------------------
--echo # Second password tests

ALTER USER arthurdent@localhost IDENTIFIED BY 'efgh' RETAIN CURRENT PASSWORD;

--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pabcd -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pabcd -e "SELECT CURRENT_USER()" 2>&1

# Remove old password
ALTER USER arthurdent@localhost DISCARD OLD PASSWORD;

--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pabcd -e "SELECT CURRENT_USER()" 2>&1
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pabcd -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pefgh -e "SELECT CURRENT_USER()" 2>&1

# Set it again
ALTER USER arthurdent@localhost IDENTIFIED BY 'ijkl' RETAIN CURRENT PASSWORD;

--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pijkl -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pijkl -e "SELECT CURRENT_USER()" 2>&1

# Change current password without removing another
ALTER USER arthurdent@localhost IDENTIFIED BY 'mnop';

--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pefgh -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pmnop -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pmnop -e "SELECT CURRENT_USER()" 2>&1

# Discard old password
ALTER USER arthurdent@localhost DISCARD OLD PASSWORD;
# Use SET PASSWORD to retain current password
SET PASSWORD FOR arthurdent@localhost = 'qrst' RETAIN CURRENT PASSWORD;

--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pmnop -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pmnop -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pqrst -e "SELECT CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=$ssl_option -uarthurdent -pqrst -e "SELECT CURRENT_USER()" 2>&1

#Password expiry
ALTER USER arthurdent@localhost DISCARD OLD PASSWORD;
ALTER USER arthurdent@localhost IDENTIFIED BY 'abcd';
ALTER USER arthurdent@localhost IDENTIFIED BY 'efgh' RETAIN CURRENT PASSWORD;
ALTER USER arthurdent@localhost PASSWORD EXPIRE;

--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pabcd --connect-expired-password -e "SELECT 1" 2>&1
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pefgh --connect-expired-password -e "SELECT 1" 2>&1

# user() function tests
ALTER USER arthurdent@localhost IDENTIFIED BY 'abcd';
GRANT APPLICATION_PASSWORD_ADMIN ON *.* TO arthurdent@localhost;

--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pabcd -e "ALTER USER USER() IDENTIFIED BY 'efgh' RETAIN CURRENT PASSWORD" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pefgh -e "SELECT 1" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pabcd -e "ALTER USER USER() DISCARD OLD PASSWORD" 2>&1
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pabcd -e "SELECT 1" 2>&1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT -uarthurdent -pefgh -e "SELECT 1" 2>&1

--echo #-----------------------------------------------------------------------
--echo # Cleanup

# Drop test users

--connection default
# Drop test users
DROP USER arthurdent@localhost;

--echo #-----------------------------------------------------------------------
