
--echo #
--echo # Bug#26429570: PASSSWORD FROM CACHE IS CLEANED FOR FAILED STATEMENT
--echo #

# Create users

CREATE USER arthurdent@localhost IDENTIFIED WITH caching_sha2_password BY 'abcd';
CREATE USER marvintheparanoidandroid@localhost IDENTIFIED WITH caching_sha2_password BY 'efghi';
CREATE USER zaphodbeeblebrox@localhost IDENTIFIED WITH caching_sha2_password BY 'xyz';

# Clear caches
FLUSH PRIVILEGES;

--echo # Try to establish a connection without TLS or RSA keys - Should fail
--error 1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1

--echo # Try to establish a connection with TLS - Should succeed
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=REQUIRED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=REQUIRED -umarvintheparanoidandroid -pefghi -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=REQUIRED -uzaphodbeeblebrox -pxyz -e "SELECT USER(), CURRENT_USER()" 2>&1

--echo # Try to establish a connection without TLS or RSA keys - Should succeed
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -umarvintheparanoidandroid -pefghi -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uzaphodbeeblebrox -pxyz -e "SELECT USER(), CURRENT_USER()" 2>&1

--echo # Using root, execute user management DDLs which would result into an error
--connection default

--error ER_PLUGIN_IS_NOT_LOADED
ALTER USER u1 IDENTIFIED WITH 'invalid_plugin' BY 'pwd';

--echo # Try to establish a connection without TLS or RSA keys - Should succeed
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -umarvintheparanoidandroid -pefghi -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uzaphodbeeblebrox -pxyz -e "SELECT USER(), CURRENT_USER()" 2>&1

--error ER_CANNOT_USER
RENAME USER non_existing1 to non_existing2;
--error ER_CANNOT_USER
ALTER USER non_existing WITH MAX_QUERIES_PER_HOUR 10;
--error ER_CANNOT_USER
DROP USER non_existing;

--echo # Try to establish a connection without TLS or RSA keys - Should succeed
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -umarvintheparanoidandroid -pefghi -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uzaphodbeeblebrox -pxyz -e "SELECT USER(), CURRENT_USER()" 2>&1

--echo # Change user attributes other than credentials
ALTER USER arthurdent@localhost WITH MAX_QUERIES_PER_HOUR 10;
GRANT SELECT ON *.* TO arthurdent@localhost;
--echo # Password cache should not have been invalidated
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1

--echo # A failed alter on multiple user should not alter cache state
--error ER_CANNOT_USER
ALTER USER non_existing1 IDENTIFIED BY 'haha', marvintheparanoidandroid@localhost IDENTIFIED BY 'hoho';
--error ER_CANT_CREATE_USER_WITH_GRANT
GRANT INSERT ON *.* TO non_existing1, marvintheparanoidandroid@localhost;
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -umarvintheparanoidandroid -pefghi -e "SELECT USER(), CURRENT_USER()" 2>&1

# Cleanup - Close connections and remove users

DROP USER arthurdent@localhost;
DROP USER marvintheparanoidandroid@localhost;
DROP USER zaphodbeeblebrox@localhost;

--echo #
--echo # Bug#26429956: CACHE IS CHECKED FOR USERS BELONGING TO OTHER PLUGINS
--echo #

# Create users

CREATE USER arthurdent@localhost IDENTIFIED WITH caching_sha2_password BY 'abcd';
CREATE USER marvintheparanoidandroid@localhost IDENTIFIED WITH caching_sha2_password BY 'efghi';
CREATE USER zaphodbeeblebrox@localhost IDENTIFIED WITH caching_sha2_password BY 'xyz';

# Clear caches
FLUSH PRIVILEGES;

CREATE USER u1@localhost IDENTIFIED WITH 'caching_sha2_password' BY 'pwd';

--echo # Try to establish a connection with TLS - Should succeed
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=REQUIRED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=REQUIRED -umarvintheparanoidandroid -pefghi -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=REQUIRED -uzaphodbeeblebrox -pxyz -e "SELECT USER(), CURRENT_USER()" 2>&1

--echo # Try to establish a connection without TLS or RSA keys - Should succeed
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -umarvintheparanoidandroid -pefghi -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uzaphodbeeblebrox -pxyz -e "SELECT USER(), CURRENT_USER()" 2>&1

ALTER USER u1@localhost WITH MAX_QUERIES_PER_HOUR 10;
ALTER USER arthurdent@localhost WITH MAX_QUERIES_PER_HOUR 10;

--echo # Try to establish a connection without TLS or RSA keys - Should succeed
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uarthurdent -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -umarvintheparanoidandroid -pefghi -e "SELECT USER(), CURRENT_USER()" 2>&1
--exec $MYSQL --host=127.0.0.1 --port=$MASTER_MYPORT --ssl-mode=DISABLED -uzaphodbeeblebrox -pxyz -e "SELECT USER(), CURRENT_USER()" 2>&1

# We let our server restart attempts write to the file $server_log.
let server_log= $MYSQLTEST_VARDIR/log/mysqld.1.err;
let SEARCH_FILE= $server_log;
--echo # Search for absence of: Possible it is deleted by other thread.
--let INVERSE_SEARCH= 1;
--let SEARCH_PATTERN= Possible it is deleted by other thread;
--source include/search_pattern_multiline.inc

# Cleanup - Close connections and remove users

DROP USER arthurdent@localhost;
DROP USER marvintheparanoidandroid@localhost;
DROP USER zaphodbeeblebrox@localhost;

