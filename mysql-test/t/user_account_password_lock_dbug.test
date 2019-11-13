#
# Test per-user-account lock
#


--source include/have_debug.inc

call mtr.add_suppression('Can not read and process value of User_attributes column from mysql.user table for user');

--echo # Test FR1.4: time expired resets the lock

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

SET GLOBAL DEBUG = '+d,account_lock_daynr_add_one';

--echo # It's still locked even after one day has passed
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

SET GLOBAL DEBUG = '-d,account_lock_daynr_add_one';

SET GLOBAL DEBUG = '+d,account_lock_daynr_add_ten';

--echo # Should fail as unlocked
disable_query_log;
--error ER_ACCESS_DENIED_ERROR
connect(errcon,localhost,foo,,test);
enable_query_log;

SET GLOBAL DEBUG = '-d,account_lock_daynr_add_ten';

DROP USER foo@localhost;


--echo Test FR12: unbounded

CREATE USER foo@localhost IDENTIFIED BY 'foo' FAILED_LOGIN_ATTEMPTS 2 PASSWORD_LOCK_TIME UNBOUNDED;
--echo # Must say UNBOUNDED
--replace_regex /AS .* REQUIRE NONE/AS <secret> REQUIRE NONE/
SHOW CREATE USER foo@localhost;

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

SET GLOBAL DEBUG = '+d,account_lock_daynr_add_ten';

--echo # It's still locked even after 10 days have passed
disable_query_log;
--error ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK
connect(errcon,localhost,foo,,test);
enable_query_log;

SET GLOBAL DEBUG = '-d,account_lock_daynr_add_ten';

DROP USER foo@localhost;


--echo # End of 8.0 tests
