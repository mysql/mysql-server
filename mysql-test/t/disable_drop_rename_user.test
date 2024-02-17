--echo #
--echo # WL#14073: Disable DROP/RENAME USER for SQL DEFINER users in procedures,
--echo #           functions, views, triggers and events.
--echo #

# This test takes long time, so only run it with the --big-test mtr-flag.
--source include/big_test.inc

--enable_connect_log

CREATE DATABASE wl14073;
USE wl14073;
CREATE table t1(i int);
# create user without ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges
CREATE USER normal_user;
GRANT ALL ON *.* TO normal_user;
REVOKE ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER
  ON *.* FROM normal_user;
# create user with all privileges.
CREATE USER power_user;
GRANT ALL ON *.* TO power_user;

--echo # case1: check for view with normal_user
--connect(normal_conn, localhost, normal_user)
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 VIEW v1 as SELECT * FROM t1;
--echo # user is referenced in view v1 so drop/rename user should fail
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER u1;
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER u1 to new_u1;
DROP VIEW v1;
--echo # user is not more referenced in view v1 so rename/drop user should pass
RENAME USER u1 to new_u1;
DROP USER new_u1;

--echo # case2: check for view with power_user
--connect(power_conn, localhost, power_user)
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 VIEW v1 as SELECT * FROM t1;
--echo # user is referenced in view v1 so rename/drop user should pass as user has ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER.
RENAME USER u1 to new_u1;
DROP USER new_u1;
DROP VIEW v1;

--echo # case3: check for event with normal_user
--connection normal_conn
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 EVENT ev1 ON SCHEDULE EVERY 5 HOUR DO SELECT 1;
--echo # user is referenced in event ev1 so drop/rename user should fail
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER u1;
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER u1 to new_u1;
DROP EVENT ev1;
--echo # user is no more referenced in event ev1 so rename/drop user should pass
RENAME USER u1 to new_u1;
DROP USER new_u1;

--echo # case4: check for event with power_user
--connection power_conn
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 EVENT ev1 ON SCHEDULE EVERY 5 HOUR DO SELECT 1;
--echo # user is referenced in event ev1 so rename/drop user should pass
RENAME USER u1 to new_u1;
DROP USER new_u1;
DROP EVENT ev1;

--echo # case5: check for procedure with normal_user
--connection normal_conn
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 PROCEDURE p1() DELETE FROM t1;
--echo # user is referenced in procedure p1, so drop/rename user should fail
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER u1;
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER u1 to new_u1;
DROP PROCEDURE p1;
--echo # user is no more referenced in procedure p1, so rename/drop user should pass
RENAME USER u1 to new_u1;
DROP USER new_u1;

--echo # case6: check for procedure with power_user
--connection power_conn
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 PROCEDURE p1() DELETE FROM t1;
--echo # user is referenced in procedure p1, so rename/drop user should pass
RENAME USER u1 to new_u1;
DROP USER new_u1;
DROP PROCEDURE p1;

--echo # case7: check for function with normal_user
--connection normal_conn
set GLOBAL log_bin_trust_function_creators=1;
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 FUNCTION f1() RETURNS INT RETURN 1;
--echo # user is referenced in function f1, so drop/rename user should fail
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER u1;
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER u1 to new_u1;
DROP FUNCTION f1;
--echo # user is no more referenced in function f1, so rename/drop user should pass
RENAME USER u1 to new_u1;
DROP USER new_u1;

--echo # case8: check for function with power_user
--connection power_conn
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 FUNCTION f1() RETURNS INT RETURN 1;
--echo # user is referenced in function f1, so rename/drop user should pass
RENAME USER u1 to new_u1;
DROP USER new_u1;
DROP FUNCTION f1;

--echo # case9: check for trigger with normal_user
--connection normal_conn
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW DELETE FROM t1;
--echo # user is referenced in trigger trig1, so drop/rename user should fail
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP USER u1;
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER u1 to new_u1;
DROP TRIGGER trig1;
--echo # user is no more referenced in trigger trig1, so rename/drop user should pass
RENAME USER u1 to new_u1;
DROP USER new_u1;

--echo # case10: check for trigger with power_user
--connection power_conn
CREATE USER u1;
USE wl14073;
CREATE DEFINER = u1 TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW DELETE FROM t1;
RENAME USER u1 to new_u1;
DROP USER new_u1;
DROP TRIGGER trig1;

--echo # case11: check CREATE USER for orphaned view for normal user/power user
--connection normal_conn
USE wl14073;
CREATE USER dummy;
# create orphaned view(view with non existing definer account)
CREATE DEFINER = u1 VIEW v1 as SELECT * FROM t1;
--error ER_NO_SUCH_USER
SELECT * FROM v1;
--echo # try creating missing definer account, should report error as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are not there
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
CREATE USER u1;
--echo # try renaming existing user to a matching definer account, it should fail.
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER dummy to u1;
--connection power_conn
USE wl14073;
--error ER_NO_SUCH_USER
SELECT * FROM v1;
--echo # try creating missing definer account, should pass as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are there
CREATE USER u1;
DROP USER u1;
--echo # try renaming existing user to a matching definer account, it should pass
RENAME USER dummy to u1;
DROP VIEW v1;
DROP USER u1;

--echo # case12: check CREATE USER for orphaned event for normal user/power user
--connection normal_conn
USE wl14073;
CREATE USER dummy;
# create orphaned event
CREATE DEFINER = u1 EVENT ev1 ON SCHEDULE EVERY 5 HOUR DO SELECT 1;
--echo # try creating missing definer account, should report error as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are not there
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
CREATE USER u1;
--echo # try renaming existing user to a matching definer account, it should fail.
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER dummy to u1;
--connection power_conn
USE wl14073;
--echo # try creating missing definer account, should pass as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are there
CREATE USER u1;
DROP USER u1;
--echo # try renaming existing user to a matching definer account, it should pass
RENAME USER dummy to u1;
DROP EVENT ev1;
DROP USER u1;

--echo # case13: check CREATE USER for orphaned procedure for normal user/power user
--connection normal_conn
USE wl14073;
CREATE USER dummy;
# create orphaned procedure
CREATE DEFINER = u1 PROCEDURE p1() DELETE FROM t1;
--error ER_NO_SUCH_USER
CALL p1();
--echo # try creating missing definer account, should report error as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privilegs are not there
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
CREATE USER u1;
--echo # try renaming existing user to a matching definer account, it should fail.
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER dummy to u1;
--connection power_conn
USE wl14073;
--error ER_NO_SUCH_USER
CALL p1();
--echo # try creating missing definer account, should pass as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are there
CREATE USER u1;
DROP USER u1;
--echo # try renaming existing user to a matching definer account, it should pass
RENAME USER dummy to u1;
DROP PROCEDURE p1;
DROP USER u1;

--echo # case14: check CREATE USER for orphaned function for normal user/power user
--connection normal_conn
USE wl14073;
CREATE USER dummy;
# create orphaned function
CREATE DEFINER = u1 FUNCTION f1() RETURNS INT RETURN 1;
--error ER_NO_SUCH_USER
SELECT f1();
--echo # try creating missing definer account, should report error as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are not there
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
CREATE USER u1;
--echo # try renaming existing user to a matching definer account, it should fail.
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER dummy to u1;
--connection power_conn
USE wl14073;
--error ER_NO_SUCH_USER
SELECT f1();
--echo # try creating missing definer account, should pass as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are there
CREATE USER u1;
DROP USER u1;
--echo # try renaming existing user to a matching definer account, it should pass
RENAME USER dummy to u1;
DROP FUNCTION f1;
DROP USER u1;

--echo # case15: check CREATE USER for orphaned trigger for normal user/power user
--connection normal_conn
USE wl14073;
CREATE USER dummy;
# create orphaned trigger
CREATE DEFINER = u1 TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW DELETE FROM t1;
--error ER_NO_SUCH_USER
INSERT INTO t1 VALUES (10);
--echo # try creating missing definer account, should report error as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are not there
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
CREATE USER u1;
--echo # try renaming existing user to a matching definer account, it should fail.
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
RENAME USER dummy to u1;
--connection power_conn
USE wl14073;
--error ER_NO_SUCH_USER
INSERT INTO t1 VALUES (10);
--echo # try creating missing definer account, should pass as ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER privileges are there
CREATE USER u1;
DROP USER u1;
--echo # try renaming existing user to a matching definer account, it should pass
RENAME USER dummy to u1;
DROP TRIGGER trig1;
DROP USER u1;

# Cleanup
--connection default
--disconnect normal_conn
--disconnect power_conn
DROP USER normal_user, power_user;

--echo # case16: check for user name case sensitivity
CREATE USER ABC;
CREATE USER ABc;
USE wl14073;
CREATE DEFINER = ABC VIEW v2 as SELECT * FROM t1;
--echo # should pass without any warnings
DROP USER ABc;
--echo # should pass with warnings
DROP USER ABC;

--echo # case17: check for host name
CREATE USER u1@192.129.12.11;
CREATE USER 'u1'@'%.com';
CREATE USER 'u1'@'abc.com';
USE wl14073;
CREATE DEFINER = u1@192.129.12.11 VIEW v3 as SELECT * FROM t1;
CREATE DEFINER = 'u1'@'%.com' VIEW v4 as SELECT * FROM t1;
--echo # check that host name is not case sensitive
CREATE DEFINER = 'u1'@'AbC.com' VIEW v5 as SELECT * FROM t1;
--echo # should not match with any definer account names
--error ER_CANNOT_USER
DROP USER 'u1'@'192.129.12.%';
--error ER_CANNOT_USER
DROP USER 'u1'@'%';
--echo # should pass with warnings
DROP USER u1@192.129.12.11;
--echo # should pass with warnings
DROP USER 'u1'@'%.com';
--echo # should pass with warnings even when hostname is specified with different case
DROP USER 'u1'@'ABC.COM';

--echo # case18: Drop multiple users being definers of multiple entity types.
CREATE USER u1;
CREATE USER u2;

USE wl14073;

CREATE DEFINER = u1 VIEW v6 as SELECT * FROM t1;
CREATE DEFINER = u1 EVENT ev1 ON SCHEDULE EVERY 5 HOUR DO SELECT 1;

CREATE DEFINER = u2 VIEW v7 as SELECT * FROM t1;
CREATE DEFINER = u2 EVENT ev2 ON SCHEDULE EVERY 5 HOUR DO SELECT 1;

--echo # Should pass with warnings for entity types 'event' and 'view' for each user.
DROP USER u1, u2;

#Cleanup
DROP DATABASE wl14073;
--disable_connect_log
