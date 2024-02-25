# Setup database, tables and user accounts
--disable_warnings
drop database if exists mysqltest_db1;
--enable_warnings
create database mysqltest_db1;
use mysqltest_db1;
create table t_column_priv_only (a int, b int);
create table t_select_priv like t_column_priv_only;
create table t_no_priv like t_column_priv_only;
create user mysqltest_u1@localhost;
grant all privileges on test.* to mysqltest_u1@localhost;
grant insert (a) on mysqltest_db1.t_column_priv_only to mysqltest_u1@localhost;
grant select on mysqltest_db1.t_select_priv to mysqltest_u1@localhost;

--echo ** Connect as restricted user mysqltest_u1.
--echo
connect (con1,localhost,mysqltest_u1,,);
connection con1;

########################################################################
--echo ** Test column level privileges only. No SELECT privileges on the table.
--echo ** INSERT INTO ... VALUES ...
--echo ** Attempting to insert values to a table with only column privileges
--echo ** should work.
insert into mysqltest_db1.t_column_priv_only (a) VALUES (1);
--echo

#########################################################################
--echo ** SHOW COLUMNS
--echo ** Should succeed because we have privileges (any) on at least one of the columns.
select column_name as 'Field',column_type as 'Type',is_nullable as 'Null',column_key as 'Key',column_default as 'Default',extra as 'Extra' from information_schema.columns where table_schema='mysqltest_db1' and table_name='t_column_priv_only';
show columns from mysqltest_db1.t_column_priv_only;
#########################################################################
--echo ** SHOW COLUMNS
--echo ** Should fail because there are no privileges on any column combination.
--error 1142
show columns from mysqltest_db1.t_no_priv;
--echo ** However, select from I_S.COLUMNS will succeed but not show anything:
select column_name as 'Field',column_type as 'Type',is_nullable as 'Null',column_key as 'Key',column_default as 'Default',extra as 'Extra' from information_schema.columns where table_schema='mysqltest_db1' and table_name='t_no_priv';
--echo
#########################################################################
--echo ** CREATE TABLE ... LIKE ... require SELECT privleges and will fail.
--error 1142
create table test.t_no_priv like mysqltest_db1.column_priv_only;
--echo
#########################################################################
--echo ** Just to be sure... SELECT also fails.
--error 1142
select * from mysqltest_db1.t_column_priv_only;
--echo
#########################################################################
--echo ** SHOW CREATE TABLE ... require any privileges on all columns (the entire table).
--echo ** First we try and fail on a table with only one column privilege.
--error 1142
show create table mysqltest_db1.t_column_priv_only;
--echo
#########################################################################
--echo ** Now we do the same on a table with SELECT privileges.
--echo
#########################################################################
--echo ** SHOW COLUMNS
--echo ** Success because we got some privileges on the table (SELECT_ACL)
show columns from mysqltest_db1.t_select_priv;
--echo
#########################################################################
--echo ** CREATE TABLE ... LIKE ... require SELECT privleges and will SUCCEED.
--disable_warnings
drop table if exists test.t_duplicated;
--enable_warnings
create table test.t_duplicated like mysqltest_db1.t_select_priv;
drop table test.t_duplicated;
--echo
#########################################################################
--echo ** SHOW CREATE TABLE will succeed because we have a privilege on all columns in the table (table-level privilege).
show create table mysqltest_db1.t_select_priv;
--echo
#########################################################################
--echo ** SHOW CREATE TABLE will fail if there is no grants at all:
--error 1142
show create table mysqltest_db1.t_no_priv;
--echo

connection default;

#
# SHOW INDEX
#
use mysqltest_db1;
CREATE TABLE t5 (s1 INT);
CREATE INDEX i ON t5 (s1);
CREATE TABLE t6 (s1 INT, s2 INT);
CREATE VIEW v5 AS SELECT * FROM t5;
CREATE VIEW v6 AS SELECT * FROM t6;
CREATE VIEW v2 AS SELECT * FROM t_select_priv;
CREATE VIEW v3 AS SELECT * FROM t_select_priv;
CREATE INDEX i ON t6 (s1);
GRANT UPDATE (s2) ON t6 to mysqltest_u1@localhost;
GRANT UPDATE (s2) ON v6 to mysqltest_u1@localhost;
GRANT SHOW VIEW ON v2 to mysqltest_u1@localhost;
GRANT SHOW VIEW, SELECT ON v3 to mysqltest_u1@localhost;
ANALYZE TABLE t6;

connection con1;
use mysqltest_db1;
--echo ** Connect as restricted user mysqltest_u1.
--echo ** SELECT FROM INFORMATION_SCHEMA.STATISTICS will succeed because any privileges will do (authentication is enough).
SELECT * FROM INFORMATION_SCHEMA.STATISTICS WHERE table_name='t5';
#
# Bug27145 EXTRA_ACL trouble
#
--echo ** SHOW INDEX FROM t5 will fail because we don't have any privileges on any column combination.
--error 1142
SHOW INDEX FROM t5;
--echo ** SHOW INDEX FROM t6 will succeed because there exist a privilege on a column combination on t6.
SHOW INDEX FROM t6;

# CHECK TABLE
--echo ** CHECK TABLE requires any privilege on any column combination and should succeed for t6:
CHECK TABLE t6;
--echo ** With no privileges access is naturally denied:
--error 1142
CHECK TABLE t5;

# CHECKSUM
--echo ** CHECKSUM TABLE requires SELECT privileges on the table. The following should fail:
--error 1142
CHECKSUM TABLE t6;
--echo ** And this should work:
CHECKSUM TABLE t_select_priv;

# SHOW CREATE VIEW
--error 1142
SHOW CREATE VIEW v5;
--error 1142
SHOW CREATE VIEW v6;
--error 1142
SHOW CREATE VIEW v2;
SHOW CREATE VIEW v3;

connection default;
disconnect con1;
drop database mysqltest_db1;
drop user mysqltest_u1@localhost;

USE test;
--echo #
--echo # Bug #28653104: HANDLE_FATAL_SIGNAL (SIG=11) IN __STRCHR_SSE2
--echo #

--echo # Setup
call mtr.add_suppression("The plugin .* used to authenticate user .* is not loaded. Nobody can currently login using this account");
call mtr.add_suppression("User entry .* has an empty plugin value. The user will be ignored and no one can login with this user anymore.");
call mtr.add_suppression("Some of the user accounts with SUPER privileges were disabled because of empty mysql.user.plugin value.");
call mtr.add_suppression("Found an entry in the 'role_edges' table with unknown authorization ID '`r3`@`%`'");

CREATE TEMPORARY TABLE save_user AS SELECT * FROM mysql.user;
CREATE TEMPORARY TABLE save_role_edges AS SELECT * FROM mysql.role_edges;

--echo # Test
INSERT INTO mysql.user(user,plugin,ssl_cipher,x509_issuer,x509_subject) VALUES ('foo','bar','','',''),('bar','bar','','',''),('baz','bar','','','');
CREATE ROLE r1,r2,r3;
GRANT r3 TO r2;
--error ER_PLUGIN_IS_NOT_LOADED
CREATE USER''@''IDENTIFIED WITH 'server' AS 'user';
UPDATE mysql.user SET plugin='';
--echo # Success criteria: must not crash
--error ER_PLUGIN_IS_NOT_LOADED
CREATE USER plug IDENTIFIED WITH 'server';

--echo # Cleanup
DELETE FROM mysql.user;
INSERT INTO mysql.user SELECT * FROM save_user;
DELETE FROM mysql.role_edges;
INSERT INTO mysql.role_edges SELECT * FROM save_role_edges;
FLUSH PRIVILEGES;

--echo #
--echo # Bug#29395197 REVOKE ALLOWS INCORRECT VALUES
--echo #
# Test with the user
CREATE USER testuser@localhost;
--error ER_SYNTAX_ERROR
GRANT abc ON *.* TO testuser@localhost;
REVOKE abc ON *.* FROM testuser@localhost;
# Test with the role
CREATE ROLE testrole;
--error ER_SYNTAX_ERROR
GRANT abc ON *.* TO testrole;
REVOKE abc ON *.* FROM testrole;
# Cleanup
DROP USER testuser@localhost;
DROP USER testrole;

--echo #
--echo # Anonymous user fail to properly aggregate EXECUTE privilege.
--echo #
CREATE USER ''@'';
GRANT EXECUTE ON PROCEDURE sys.table_exists TO ''@'';
GRANT SELECT ON db1.* TO ''@'';
SHOW GRANTS FOR ''@'';
DROP USER ''@'';

--echo # End of 8.0 tests

