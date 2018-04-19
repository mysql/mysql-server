#
# Bug#26429570: PASSSWORD FROM CACHE IS CLEANED FOR FAILED STATEMENT
#
CREATE USER arthurdent@localhost IDENTIFIED WITH caching_sha2_password BY 'abcd';
CREATE USER marvintheparanoidandroid@localhost IDENTIFIED WITH caching_sha2_password BY 'efghi';
CREATE USER zaphodbeeblebrox@localhost IDENTIFIED WITH caching_sha2_password BY 'xyz';
FLUSH PRIVILEGES;
# Try to establish a connection without TLS or RSA keys - Should fail
mysql: [Warning] Using a password on the command line interface can be insecure.
ERROR 2061 (HY000): Authentication plugin 'caching_sha2_password' reported error: Authentication requires secure connection.
# Try to establish a connection with TLS - Should succeed
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
arthurdent@localhost	arthurdent@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
marvintheparanoidandroid@localhost	marvintheparanoidandroid@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
zaphodbeeblebrox@localhost	zaphodbeeblebrox@localhost
# Try to establish a connection without TLS or RSA keys - Should succeed
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
arthurdent@localhost	arthurdent@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
marvintheparanoidandroid@localhost	marvintheparanoidandroid@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
zaphodbeeblebrox@localhost	zaphodbeeblebrox@localhost
# Using root, execute user management DDLs which would result into an error
ALTER USER u1 IDENTIFIED WITH 'invalid_plugin' BY 'pwd';
ERROR HY000: Plugin 'invalid_plugin' is not loaded
# Try to establish a connection without TLS or RSA keys - Should succeed
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
arthurdent@localhost	arthurdent@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
marvintheparanoidandroid@localhost	marvintheparanoidandroid@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
zaphodbeeblebrox@localhost	zaphodbeeblebrox@localhost
RENAME USER non_existing1 to non_existing2;
ERROR HY000: Operation RENAME USER failed for 'non_existing1'@'%'
ALTER USER non_existing WITH MAX_QUERIES_PER_HOUR 10;
ERROR HY000: Operation ALTER USER failed for 'non_existing'@'%'
DROP USER non_existing;
ERROR HY000: Operation DROP USER failed for 'non_existing'@'%'
# Try to establish a connection without TLS or RSA keys - Should succeed
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
arthurdent@localhost	arthurdent@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
marvintheparanoidandroid@localhost	marvintheparanoidandroid@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
zaphodbeeblebrox@localhost	zaphodbeeblebrox@localhost
# Change user attributes other than credentials
ALTER USER arthurdent@localhost WITH MAX_QUERIES_PER_HOUR 10;
GRANT SELECT ON *.* TO arthurdent@localhost;
# Password cache should not have been invalidated
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
arthurdent@localhost	arthurdent@localhost
# A failed alter on multiple user should not alter cache state
ALTER USER non_existing1 IDENTIFIED BY 'haha', marvintheparanoidandroid@localhost IDENTIFIED BY 'hoho';
ERROR HY000: Operation ALTER USER failed for 'non_existing1'@'%'
GRANT INSERT ON *.* TO non_existing1, marvintheparanoidandroid@localhost;
ERROR 42000: You are not allowed to create a user with GRANT
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
marvintheparanoidandroid@localhost	marvintheparanoidandroid@localhost
DROP USER arthurdent@localhost;
DROP USER marvintheparanoidandroid@localhost;
DROP USER zaphodbeeblebrox@localhost;
#
# Bug#26429956: CACHE IS CHECKED FOR USERS BELONGING TO OTHER PLUGINS
#
CREATE USER arthurdent@localhost IDENTIFIED WITH caching_sha2_password BY 'abcd';
CREATE USER marvintheparanoidandroid@localhost IDENTIFIED WITH caching_sha2_password BY 'efghi';
CREATE USER zaphodbeeblebrox@localhost IDENTIFIED WITH caching_sha2_password BY 'xyz';
FLUSH PRIVILEGES;
CREATE USER u1@localhost IDENTIFIED WITH 'mysql_native_password' BY 'pwd';
# Try to establish a connection with TLS - Should succeed
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
arthurdent@localhost	arthurdent@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
marvintheparanoidandroid@localhost	marvintheparanoidandroid@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
zaphodbeeblebrox@localhost	zaphodbeeblebrox@localhost
# Try to establish a connection without TLS or RSA keys - Should succeed
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
arthurdent@localhost	arthurdent@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
marvintheparanoidandroid@localhost	marvintheparanoidandroid@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
zaphodbeeblebrox@localhost	zaphodbeeblebrox@localhost
ALTER USER u1@localhost WITH MAX_QUERIES_PER_HOUR 10;
ALTER USER arthurdent@localhost WITH MAX_QUERIES_PER_HOUR 10;
# Try to establish a connection without TLS or RSA keys - Should succeed
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
arthurdent@localhost	arthurdent@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
marvintheparanoidandroid@localhost	marvintheparanoidandroid@localhost
mysql: [Warning] Using a password on the command line interface can be insecure.
USER()	CURRENT_USER()
zaphodbeeblebrox@localhost	zaphodbeeblebrox@localhost
# Search for absence of: Possible it is deleted by other thread.
DROP USER arthurdent@localhost;
DROP USER marvintheparanoidandroid@localhost;
DROP USER zaphodbeeblebrox@localhost;
