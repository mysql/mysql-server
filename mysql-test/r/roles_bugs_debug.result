#
# Bug#28395115: permission denied if grants are given through role
#
CREATE DATABASE my_db;
CREATE table my_db.t1 (id int primary key);
CREATE ROLE foo_role;
CREATE USER foo, bar;
GRANT INSERT(id), UPDATE(id), SELECT(id) ON my_db.t1 to foo_role, bar;
GRANT EXECUTE, SYSTEM_VARIABLES_ADMIN ON *.* TO foo, bar;
GRANT foo_role TO foo;
SET DEFAULT ROLE foo_role TO foo;
SET DEBUG_SYNC='in_check_grant_all_columns SIGNAL s1 WAIT_FOR s2';
# Inserts are now allowed if grants are given through role
INSERT into my_db.t1 values(8) on duplicate key UPDATE id = values(id) + 80;
# Now revoke all privileges from the role
SET DEBUG_SYNC='now WAIT_FOR s1';
SET DEBUG_SYNC='after_table_grant_revoke SIGNAL s2';
REVOKE ALL ON my_db.t1 FROM foo_role;
# Despite all privileges are revoked current SQL statement will succeed.
Warnings:
Warning	1287	'VALUES function' is deprecated and will be removed in a future release. Please use an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON DUPLICATE KEY UPDATE clause with alias.col instead
SET DEBUG_SYNC= 'RESET';
# But the subsequent statement will fail.
INSERT into my_db.t1 values(9) on duplicate key UPDATE id = values(id) + 90;
ERROR 42000: INSERT, UPDATE command denied to user 'foo'@'localhost' for table 't1'
SET DEBUG_SYNC='in_check_grant_all_columns SIGNAL s1 WAIT_FOR s2';
# Inserts are now allowed if grants are given through role
INSERT into my_db.t1 values(6) on duplicate key UPDATE id = values(id) + 60;
# Now revoke all privileges from the user
SET DEBUG_SYNC='now WAIT_FOR s1';
SET DEBUG_SYNC='after_table_grant_revoke SIGNAL s2';
REVOKE ALL ON my_db.t1 FROM bar;
# Since all privileges are revoked therefore current SQL statement will fail.
ERROR 42000: INSERT command denied to user 'bar'@'localhost' for column 'id' in table 't1'
# Subsequent statement will fail as well.
INSERT into my_db.t1 values(9) on duplicate key UPDATE id = values(id) + 90;
ERROR 42000: INSERT, UPDATE command denied to user 'bar'@'localhost' for table 't1'
# Cleanup
SET DEBUG_SYNC= 'RESET';
DROP DATABASE my_db;
DROP USER foo, bar;
DROP ROLE foo_role;

# End of 8.0 tests

