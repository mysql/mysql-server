--echo #
--echo # Test for WL#15874: Separate privileges for definer object creation
--echo #   and orphan object protection from SET_USER_ID and
--echo #   deprecate SET_USER_ID
--echo #
--echo # Also test WL#15875: Remove the deprecated SET_USER_ID privilege
--echo #
--source include/big_test.inc
--source include/not_valgrind.inc
--let $MYSQLD_DATADIR= `select @@datadir`
--source include/mysql_upgrade_preparation.inc

--echo # Test FR3

--echo # Test: FR8: Should have no result rows
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SET_USER_ID' ORDER BY 1;

--echo # Test: FR8: Should include root
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SET_ANY_DEFINER' ORDER BY 1;

--echo # Test: FR8: Should include root
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'ALLOW_NONEXISTENT_DEFINER' ORDER BY 1;

--echo # set grants as it used to be pre-worklog
CREATE USER wl15874@localhost;

--echo # should fail: no such grant.
--error ER_SYNTAX_ERROR
GRANT SET_USER_ID ON *.* TO wl15874@localhost;

REVOKE SET_ANY_DEFINER,ALLOW_NONEXISTENT_DEFINER ON *.* FROM root@localhost;
INSERT INTO mysql.global_grants(USER,HOST,PRIV, WITH_GRANT_OPTION)
  VALUES ('wl15874', 'localhost', 'SET_USER_ID', 'Y');
INSERT INTO mysql.global_grants(USER,HOST,PRIV, WITH_GRANT_OPTION)
  VALUES ('root', 'localhost', 'SET_USER_ID', 'Y');

--let $restart_parameters = restart:--upgrade=FORCE
--let $wait_counter= 10000
--source include/restart_mysqld.inc

--echo # Restart server with defaults
--let $restart_parameters = restart:
--source include/restart_mysqld.inc

--echo # should fail after a restart and set_user_id present: no such grant.
--error ER_SYNTAX_ERROR
GRANT SET_USER_ID ON *.* TO wl15874@localhost;

--echo # Test FR4: SET_USER_ID deprecated at startup. Must be 0: deprecation gone
select COUNT(*) FROM performance_schema.error_log
  WHERE PRIO='Warning' AND DATA REGEXP 'SET_USER_ID.*deprecated';

--echo # WL#15875 FR4: should return 0 rows: SET_USER_ID gone
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SET_USER_ID' ORDER BY 1;

--echo # FR3: should return 2 rows: root and wl15874
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SET_ANY_DEFINER' ORDER BY 1;

--echo # FR3: should return 2 rows: root and wl15874
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'ALLOW_NONEXISTENT_DEFINER' ORDER BY 1;

--echo # set grants to test FR3.1
REVOKE ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER ON *.* FROM wl15874@localhost;
REVOKE ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER ON *.* FROM root@localhost;
GRANT SUPER ON *.* TO wl15874@localhost WITH GRANT OPTION;

--echo # FR3.1: should return 0 rows
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SET_USER_ID' ORDER BY 1;

--echo # FR3.1: should return 0 rows
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SET_ANY_DEFINER' ORDER BY 1;

--echo # FR3.1: should return 0 rows
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'ALLOW_NONEXISTENT_DEFINER' ORDER BY 1;

--echo # FR3.1: should return 3 rows: root, mysql.session and wl15874
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SUPER' ORDER BY 1;

--echo # Upgrade to test FR3.1
--let $restart_parameters = restart:--upgrade=FORCE
--let $wait_counter= 10000
--source include/restart_mysqld.inc

--echo # Restart server with defaults to test FR3.1
--let $restart_parameters = restart:
--source include/restart_mysqld.inc

--echo # FR3.1: should return 0 rows
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SET_USER_ID' ORDER BY 1;

--echo # FR3.1: should return 3 rows: root, mysql.session and wl15874
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'SET_ANY_DEFINER' ORDER BY 1;

--echo # FR3.1: should return 3 rows: root, mysql.session and wl15874
SELECT GRANTEE FROM INFORMATION_SCHEMA.USER_PRIVILEGES
WHERE PRIVILEGE_TYPE = 'ALLOW_NONEXISTENT_DEFINER' ORDER BY 1;

--echo # Cleanup

REVOKE ALLOW_NONEXISTENT_DEFINER,SET_ANY_DEFINER ON *.* FROM wl15874@localhost;
REVOKE SUPER ON *.* FROM wl15874@localhost;
REVOKE SET_ANY_DEFINER, ALLOW_NONEXISTENT_DEFINER ON *.* FROM 'mysql.session'@'localhost';
DROP USER wl15874@localhost;

--source include/mysql_upgrade_cleanup.inc

--echo # End of 8.2 tests
