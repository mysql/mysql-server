set @orig_sql_mode_session= @@SESSION.sql_mode;
set @orig_sql_mode_global= @@GLOBAL.sql_mode;
create database d1;
create user 'sample'@'localhost' identified by 'password';
grant all on d1.* to 'sample'@'localhost';
flush privileges;
select database();
database()
d1
create database d2;
ERROR 42000: Access denied for user 'sample'@'localhost' to database 'd2'
create database D1;
ERROR 42000: Access denied for user 'sample'@'localhost' to database 'D1'
drop user 'sample'@'localhost';
drop database if exists d1;
CREATE DATABASE d1;
USE d1;
CREATE USER user_1@localhost;
CREATE TABLE T1(f1 INT);
CREATE TABLE t1(f1 INT);
GRANT SELECT ON T1 to user_1@localhost;
select * from t1;
ERROR 42000: SELECT command denied to user 'user_1'@'localhost' for table 't1'
select * from T1;
f1
GRANT SELECT ON t1 to user_1@localhost;
select * from information_schema.table_privileges;
GRANTEE	TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	PRIVILEGE_TYPE	IS_GRANTABLE
'user_1'@'localhost'	def	d1	T1	SELECT	NO
'user_1'@'localhost'	def	d1	t1	SELECT	NO
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user_1@localhost;
DROP USER user_1@localhost;
DROP DATABASE d1;
USE test;
CREATE DATABASE db1;
USE db1;
CREATE PROCEDURE p1() BEGIN END;
CREATE FUNCTION f1(i INT) RETURNS INT RETURN i+1;
CREATE USER user_1@localhost, USER_1@localhost;
GRANT USAGE ON db1.* to user_1@localhost;
GRANT EXECUTE ON PROCEDURE db1.P1 to user_1@localhost;
GRANT EXECUTE ON FUNCTION db1.f1 to user_1@localhost;
GRANT UPDATE ON db1.* to USER_1@localhost;
call p1();
call P1();
select f1(1);
f1(1)
2
call p1();
ERROR 42000: execute command denied to user 'USER_1'@'localhost' for routine 'db1.p1'
call P1();
ERROR 42000: execute command denied to user 'USER_1'@'localhost' for routine 'db1.P1'
select f1(1);
ERROR 42000: execute command denied to user 'USER_1'@'localhost' for routine 'db1.f1'
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user_1@localhost;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM USER_1@localhost;
DROP FUNCTION f1;
DROP PROCEDURE p1;
DROP USER user_1@localhost;
DROP USER USER_1@localhost;
DROP DATABASE db1;
use test;
set GLOBAL sql_mode= @orig_sql_mode_global;
set SESSION sql_mode= @orig_sql_mode_session;
#
# Extra test coverage for Bug#56595 RENAME TABLE causes assert on OS X
#
CREATE TABLE t1(a INT);
CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW SET new.a= 1;
RENAME TABLE t1 TO T1;
ALTER TABLE T1 RENAME t1;
DROP TABLE t1;
#
# Bug #27309094: REJECT LCTN CHANGING AFTER --INITIALIZE
#
# Verify that we are not allowed to restart the server with a
# lower_case_table_names setting different than the setting
# use during --initialize.
#
# 1. Shut server down.
# 2. initialize the server.
# 3. Restart the server with a different lower_case_table_names setting. Should fail.
# 4. Look for error.
#    Found lower_case_table_names error 1 times.
# 5. Restart the server against default datadir.
# 6. Cleanup.
#    Delete $DDIR.
#    Delete sql files.
#    Delete log files.
#    Delete expect files.
