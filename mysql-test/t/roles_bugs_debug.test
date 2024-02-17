# Save the initial number of concurrent sessions
--source include/count_sessions.inc
--source include/have_debug.inc

--echo #
--echo # Bug#28395115: permission denied if grants are given through role
--echo #
# We are verifying the after effects of revoking a privilege from a user and
# a role from another session while a user is in the middle of executing current
# SQL statement. Expected Behavior :
# 1. In case of privileges are revoked from a role changes are invisible
#    until next SQL statement is executed.
# 2. In case of privileges are revokes from a user changes are visible
#    immediately to the current SQL statement.

# Setup
CREATE DATABASE my_db;
CREATE table my_db.t1 (id int primary key);
CREATE ROLE foo_role;
CREATE USER foo, bar;
# Grant required column privileges to a role and user.
GRANT INSERT(id), UPDATE(id), SELECT(id) ON my_db.t1 to foo_role, bar;
GRANT EXECUTE, SYSTEM_VARIABLES_ADMIN ON *.* TO foo, bar;
GRANT foo_role TO foo;
SET DEFAULT ROLE foo_role TO foo;

--connect(foo_con, localhost, foo,,,)
SET DEBUG_SYNC='in_check_grant_all_columns SIGNAL s1 WAIT_FOR s2';
--echo # Inserts are now allowed if grants are given through role
send INSERT into my_db.t1 values(8) on duplicate key UPDATE id = values(id) + 80;

connection default;
--echo # Now revoke all privileges from the role
SET DEBUG_SYNC='now WAIT_FOR s1';
SET DEBUG_SYNC='after_table_grant_revoke SIGNAL s2';
REVOKE ALL ON my_db.t1 FROM foo_role;
connection foo_con;
--echo # Despite all privileges are revoked current SQL statement will succeed.
reap;
SET DEBUG_SYNC= 'RESET';
--echo # But the subsequent statement will fail.
--error ER_TABLEACCESS_DENIED_ERROR
INSERT into my_db.t1 values(9) on duplicate key UPDATE id = values(id) + 90;


--connect(bar_con, localhost, bar,,,)
SET DEBUG_SYNC='in_check_grant_all_columns SIGNAL s1 WAIT_FOR s2';
--echo # Inserts are now allowed if grants are given through role
send INSERT into my_db.t1 values(6) on duplicate key UPDATE id = values(id) + 60;
connection default;
--echo # Now revoke all privileges from the user
SET DEBUG_SYNC='now WAIT_FOR s1';
SET DEBUG_SYNC='after_table_grant_revoke SIGNAL s2';
REVOKE ALL ON my_db.t1 FROM bar;
connection bar_con;
--echo # Since all privileges are revoked therefore current SQL statement will fail.
--error ER_COLUMNACCESS_DENIED_ERROR
reap;
--echo # Subsequent statement will fail as well.
--error ER_TABLEACCESS_DENIED_ERROR
INSERT into my_db.t1 values(9) on duplicate key UPDATE id = values(id) + 90;

--echo # Cleanup
connection default;
SET DEBUG_SYNC= 'RESET';
disconnect foo_con;
disconnect bar_con;
DROP DATABASE my_db;
DROP USER foo, bar;
DROP ROLE foo_role;


--echo #
--echo # Bug #35471453: MySQL debug server stops when executing query
--echo #

CREATE USER b35471453@localhost;
GRANT CREATE ROLE, DROP ROLE ON *.* TO b35471453@localhost;
--connect(b35471453_con, localhost, b35471453,,,)
CREATE TABLE t35471453(c1 INT);
--echo # test1: should fail
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
CREATE OR REPLACE DEFINER = 'role_35471453' VIEW v35471453
  AS TABLE t35471453;
--echo # Now grant SET_ANY_DEFINER and ALLOW_NONEXISTENT_DEFINER and retry
connection default;
GRANT SET_ANY_DEFINER, ALLOW_NONEXISTENT_DEFINER ON *.* TO b35471453@localhost;
connection b35471453_con;
--echo # test2: should complete with a warning
CREATE OR REPLACE DEFINER = 'role_35471453' VIEW v35471453
  AS TABLE t35471453;
--echo # Now revoke SET_ANY_DEFINER and ALLOW_NONEXISTENT_DEFINER and try dropping the role
connection default;
REVOKE SET_ANY_DEFINER, ALLOW_NONEXISTENT_DEFINER ON *.* FROM b35471453@localhost;
connection b35471453_con;

--echo # test3: should fail
--error ER_CANNOT_USER_REFERENCED_AS_DEFINER
DROP ROLE IF EXISTS role_35471453;
--echo # Now grant SET_ANY_DEFINER and ALLOW_NONEXISTENT_DEFINER and retry
connection default;
GRANT SET_ANY_DEFINER, ALLOW_NONEXISTENT_DEFINER ON *.* TO b35471453@localhost;
connection b35471453_con;
--echo # test4: should pass with a warning
DROP ROLE IF EXISTS role_35471453;

DROP VIEW v35471453;
DROP TABLE t35471453;
connection default;
disconnect b35471453_con;

REVOKE CREATE ROLE, DROP ROLE, SET_ANY_DEFINER, ALLOW_NONEXISTENT_DEFINER ON *.* FROM b35471453@localhost;
DROP USER b35471453@localhost;


--echo
--echo # End of 8.0 tests
--echo

# Wait till we reached the initial number of concurrent sessions
--source include/wait_until_count_sessions.inc
