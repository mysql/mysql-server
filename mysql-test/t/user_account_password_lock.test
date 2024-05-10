#
# Test per-user-account lock
#

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

call mtr.add_suppression('Can not read and process value of User_attributes column from mysql.user table for user');

CREATE USER foo@localhost IDENTIFIED BY 'foo';

--echo # Should be empty
SELECT user_attributes FROM mysql.user WHERE user='foo';

--echo # Should fail
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # Should fail with the same error
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

UPDATE mysql.user SET user_attributes='{"Password_locking": {"failed_login_attempts": 2, "password_lock_time_days": 2}}' WHERE user='foo';
FLUSH PRIVILEGES;

SELECT user_attributes FROM mysql.user WHERE user='foo';

--echo # Should still fail with policy
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # FR5: Should still fail with policy because of locked
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

DROP USER foo@localhost;
--echo # FR1
CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 4 PASSWORD_LOCK_TIME 6;
--echo # Test FR2 : should have the composite JSON and 2 subkeys
SELECT user_attributes FROM mysql.user WHERE user='foo';

--echo # FR1.1
ALTER USER foo@localhost FAILED_LOGIN_ATTEMPTS 2;
SELECT user_attributes FROM mysql.user WHERE user='foo';
ALTER USER foo@localhost PASSWORD_LOCK_TIME 3;
SELECT user_attributes FROM mysql.user WHERE user='foo';

--echo # Should still fail with policy
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # FR1.2 Should still fail with policy because of locked
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

ALTER USER foo@localhost IDENTIFIED BY 'foo';

--echo # FR1.5 Should fail locked even after ALTER USER
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

FLUSH PRIVILEGES;

--echo # FR1.3 Should fail unlocked after FLUSH PRIVILEGES
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # Should still fail with policy because of locked
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # FR1.8 Successful login doesn't work when account is locked.
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcont,localhost,foo,foo,test);
enable_query_log;

--echo # FR11 ACCOUNT UNLOCK will remove the password failed lock too.
ALTER USER foo@localhost ACCOUNT UNLOCK;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # we lock foo user account
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

CREATE USER bar@localhost IDENTIFIED BY 'bar';

--echo # bar should fail as unlocked: policy doesn't apply to it
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,bar,,test);
enable_query_log;

# Cleanup
connection default;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

DROP USER foo@localhost;
DROP USER bar@localhost;


--echo # Test SHOW CREATE USER

CREATE USER foo@localhost FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;
--echo # Must contain FAILED_LOGIN_ATTEMPTS and PASSWORD_LOCK_TIME
SHOW CREATE USER foo@localhost;
CREATE USER failed_login_attempts@localhost FAILED_LOGIN_ATTEMPTS 2;
--echo # FR3 Must contain FAILED_LOGIN_ATTEMPTS
--echo # FR9 missing PASSWORD_LOCK_TIME: assume zero
SHOW CREATE USER failed_login_attempts@localhost;
CREATE USER password_lock_time@localhost PASSWORD_LOCK_TIME 3;
--echo # FR3 Must contain PASSWORD_LOCK_TIME
--echo # FR10 missing FAILED_LOGIN_ATTEMPTS: assume zero
SHOW CREATE USER password_lock_time@localhost;

--echo # Show create user must be replayable
--let $file = $MYSQLTEST_VARDIR/tmp/wl3515.sql
--exec $MYSQL test -N -e "SHOW CREATE USER foo@localhost" > $file
DROP USER foo@localhost;
--echo # Should not fail
--exec $MYSQL test < $file 2>&1
--echo # cleanup SHOW CREATE
remove_file $file;
DROP USER foo@localhost, failed_login_attempts@localhost, password_lock_time@localhost;


--echo # Test FR1.6

CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # we lock foo user account
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

ALTER USER foo@localhost FAILED_LOGIN_ATTEMPTS 0;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # Should fail as unlocked: tracking turned off by login attempts 0
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--replace_regex /AS .* REQUIRE NONE/AS <secret> REQUIRE NONE/
SHOW CREATE USER foo@localhost;

ALTER USER foo@localhost FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 0;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # Should fail as unlocked: tracking turned off by password lock time 0
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--replace_regex /AS .* REQUIRE NONE/AS <secret> REQUIRE NONE/
SHOW CREATE USER foo@localhost;

DROP USER foo@localhost;

--echo # Test FR1.7

CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # we lock foo user account
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

ALTER USER foo@localhost FAILED_LOGIN_ATTEMPTS 2;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # Should fail as locked
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

--replace_regex /AS .* REQUIRE NONE/AS <secret> REQUIRE NONE/
SHOW CREATE USER foo@localhost;

ALTER USER foo@localhost PASSWORD_LOCK_TIME 3;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # Should fail as locked
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

--replace_regex /AS .* REQUIRE NONE/AS <secret> REQUIRE NONE/
SHOW CREATE USER foo@localhost;

DROP USER foo@localhost;


--echo # Test FR1.9: a successful login resets the failed count

CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

connect(con1,localhost,foo,foo,test);
connection default;
disconnect con1;
# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

--echo # Should fail as unlocked: the first failure count removed by a connect
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # Should fail as locked
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

DROP USER foo@localhost;


--echo # Test FR2.1: invalid JSON values

CREATE USER foo@localhost;

--echo # Test wrong composite type
UPDATE mysql.user SET user_attributes='{"Password_locking": 1}' WHERE user='foo';
FLUSH PRIVILEGES;
SELECT user,host,user_attributes FROM mysql.user WHERE user='foo';
--error ER_CANNOT_USER
SHOW CREATE USER foo@localhost;

--echo # test negative login attempts
UPDATE mysql.user SET user_attributes='{"Password_locking": {"failed_login_attempts": -2, "password_lock_time_days": 2}}' WHERE user='foo';
FLUSH PRIVILEGES;
--error ER_CANNOT_USER
SHOW CREATE USER foo@localhost;

--echo # test negative password lock time
UPDATE mysql.user SET user_attributes='{"Password_locking": {"failed_login_attempts": 2, "password_lock_time_days": -2}}' WHERE user='foo';
FLUSH PRIVILEGES;
--error ER_CANNOT_USER
SHOW CREATE USER foo@localhost;

--echo # test wrong login attempts
UPDATE mysql.user SET user_attributes='{"Password_locking": {"failed_login_attempts": "2", "password_lock_time_days": 2}}' WHERE user='foo';
FLUSH PRIVILEGES;
--error ER_CANNOT_USER
SHOW CREATE USER foo@localhost;

--echo # test wrong password lock time
UPDATE mysql.user SET user_attributes='{"Password_locking": {"failed_login_attempts": 2, "password_lock_time_days": "2"}}' WHERE user='foo';
FLUSH PRIVILEGES;
--error ER_CANNOT_USER
SHOW CREATE USER foo@localhost;

--echo # test missing login attempts
UPDATE mysql.user SET user_attributes='{"Password_locking": {"password_lock_time_days": 2}}' WHERE user='foo';
FLUSH PRIVILEGES;
SHOW CREATE USER foo@localhost;

--echo # test missing password lock time
UPDATE mysql.user SET user_attributes='{"Password_locking": {"failed_login_attempts": 2}}' WHERE user='foo';
FLUSH PRIVILEGES;
SHOW CREATE USER foo@localhost;

--echo # cleanup
UPDATE mysql.user SET user_attributes=NULL WHERE user='foo';
FLUSH PRIVILEGES;
DROP USER foo@localhost;


--echo # Test FR6: password auto-lock and account lock

CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3 ACCOUNT LOCK;

--echo # Should fail with account locked
disable_query_log;
--error ER_ACCOUNT_HAS_BEEN_LOCKED
connect(errcon,localhost,foo,foo,test);
enable_query_log;

--echo # Should fail with access denied
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

--echo # FR6: Should still fail with policy because of locked
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

DROP USER foo@localhost;


--echo # FR7: check locking and proxies

CREATE USER proxied_to_user@localhost IDENTIFIED WITH 'caching_sha2_password' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;
CREATE USER proxy_user@localhost IDENTIFIED WITH 'caching_sha2_password' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;
GRANT PROXY ON proxied_to_user@localhost TO proxy_user@localhost;

connect(con1,localhost,proxy_user,,test);
SELECT USER(), CURRENT_USER(), @@session.proxy_user;
connection default;
disconnect con1;
# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,proxy_user,proxy_user,test);
enable_query_log;

--echo # FR8: Should fail as locked even with native auth
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,proxy_user,proxy_user,test);
enable_query_log;

--echo # Should fail as unlocked: different user locked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,proxied_to_user,proxied_to_user,test);
enable_query_log;

REVOKE PROXY ON proxied_to_user@localhost FROM proxy_user@localhost;
DROP USER proxied_to_user@localhost, proxy_user@localhost;

--echo # Test COM_CHANGE_USER

CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 2;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
change_user foo,,test;
enable_query_log;

--echo # we lock foo user account
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
change_user foo,,test;
enable_query_log;

ALTER USER foo@localhost ACCOUNT UNLOCK;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
change_user foo,,test;
enable_query_log;

DROP USER foo@localhost;

--echo # Test for bug #30465813: FAILED_LOGIN_ATTEMPTS CLAUSE ERASES OTHER ATTRIBUTES
--echo #   FROM USER_ATTRIBUTES FIELD

SET GLOBAL partial_revokes=1;
CREATE USER u1 identified by 'pwd';
GRANT CREATE ON *.* TO u1;
REVOKE CREATE ON mysql.* FROM u1;
SELECT user_attributes FROM mysql.user WHERE USER = 'u1';
ALTER USER u1 FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;
--echo # Should still contain the restrictions.
SELECT user_attributes FROM mysql.user WHERE USER = 'u1';
ALTER USER u1 FAILED_LOGIN_ATTEMPTS 0 PASSWORD_LOCK_TIME 0;
--echo # Should contain only the restrictions.
SELECT user_attributes FROM mysql.user WHERE USER = 'u1';
DROP USER u1;
SET GLOBAL partial_revokes=DEFAULT;

--echo # Test for FR13: limits

--error ER_PARSE_ERROR
CREATE USER foo@localhost FAILED_LOGIN_ATTEMPTS -1;

--error ER_WRONG_VALUE
CREATE USER goo@localhost FAILED_LOGIN_ATTEMPTS 32768;

CREATE USER foo@localhost FAILED_LOGIN_ATTEMPTS 32767;
DROP USER foo@localhost;

--error ER_PARSE_ERROR
CREATE USER foo@localhost PASSWORD_LOCK_TIME -1;

--error ER_WRONG_VALUE
CREATE USER goo@localhost PASSWORD_LOCK_TIME 32768;

CREATE USER foo@localhost PASSWORD_LOCK_TIME 32767;
DROP USER foo@localhost;


--echo # Bug #30481379: TEMPORARY ACCOUNT LOCK DOES NOT WORK FOR ANONYMOUS USER

CREATE USER ''@localhost IDENTIFIED BY 'pwd' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME 3;

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,non-existent,,test);
enable_query_log;

--echo # we lock foo user account
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,non-existent2,,test);
enable_query_log;

DROP USER ''@localhost;

--echo #
--echo # user account password in conjunction with other user attributes and annotations
--echo #
CREATE USER foo@localhost IDENTIFIED BY 'foo' PASSWORD_LOCK_TIME 3 FAILED_LOGIN_ATTEMPTS 2;
ALTER USER foo@localhost ATTRIBUTE "{ \"test\": \"account locking\" }";
ALTER USER foo@localhost COMMENT "This is a test account for verifying that password locking and user attributes won't interfer with one and another.";

SELECT user_attributes FROM mysql.user WHERE user='foo';

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
change_user foo,,test;
enable_query_log;

--echo # we lock foo user account
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
change_user foo,,test;
enable_query_log;

ALTER USER foo@localhost ACCOUNT UNLOCK;
--echo # Check that we idn't drop the COMMENT or METADATA
SELECT user_attributes FROM mysql.user WHERE user='foo';

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
change_user foo,,test;
enable_query_log;

DROP USER foo@localhost;

--echo # End of 8.0 tests
