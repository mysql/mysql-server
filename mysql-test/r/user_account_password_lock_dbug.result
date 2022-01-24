call mtr.add_suppression('Can not read and process value of User_attributes column from mysql.user table for user');
# Test FR1.4: time expired resets the lock
CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;
# Should fail as unlocked
ERROR 28000: Access denied for user 'foo'@'localhost' (using password: NO)
# we lock foo user account
ERROR HY000: Access denied for user 'foo'@'localhost'. Account is blocked for 3 day(s) (3 day(s) remaining) due to 2 consecutive failed logins.
SET GLOBAL DEBUG = '+d,account_lock_daynr_add_one';
# It's still locked even after one day has passed
ERROR HY000: Access denied for user 'foo'@'localhost'. Account is blocked for 3 day(s) (2 day(s) remaining) due to 2 consecutive failed logins.
SET GLOBAL DEBUG = '-d,account_lock_daynr_add_one';
SET GLOBAL DEBUG = '+d,account_lock_daynr_add_ten';
# Should fail as unlocked
ERROR 28000: Access denied for user 'foo'@'localhost' (using password: NO)
SET GLOBAL DEBUG = '-d,account_lock_daynr_add_ten';
DROP USER foo@localhost;
Test FR12: unbounded
CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME UNBOUNDED;
# Must say UNBOUNDED
SHOW CREATE USER foo@localhost;
CREATE USER for foo@localhost
CREATE USER `foo`@`localhost` IDENTIFIED WITH 'caching_sha2_password' AS <secret> REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME UNBOUNDED
# Should fail as unlocked
ERROR 28000: Access denied for user 'foo'@'localhost' (using password: NO)
# we lock foo user account
ERROR HY000: Access denied for user 'foo'@'localhost'. Account is blocked for unlimited day(s) (unlimited day(s) remaining) due to 2 consecutive failed logins.
SET GLOBAL DEBUG = '+d,account_lock_daynr_add_ten';
# It's still locked even after 10 days have passed
ERROR HY000: Access denied for user 'foo'@'localhost'. Account is blocked for unlimited day(s) (unlimited day(s) remaining) due to 2 consecutive failed logins.
SET GLOBAL DEBUG = '-d,account_lock_daynr_add_ten';
DROP USER foo@localhost;
# End of 8.0 tests
