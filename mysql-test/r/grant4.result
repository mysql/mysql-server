drop database if exists mysqltest_db1;
create database mysqltest_db1;
use mysqltest_db1;
create table t_column_priv_only (a int, b int);
create table t_select_priv like t_column_priv_only;
create table t_no_priv like t_column_priv_only;
create user mysqltest_u1@localhost;
grant all privileges on test.* to mysqltest_u1@localhost;
grant insert (a) on mysqltest_db1.t_column_priv_only to mysqltest_u1@localhost;
grant select on mysqltest_db1.t_select_priv to mysqltest_u1@localhost;
** Connect as restricted user mysqltest_u1.

** Test column level privileges only. No SELECT privileges on the table.
** INSERT INTO ... VALUES ...
** Attempting to insert values to a table with only column privileges
** should work.
insert into mysqltest_db1.t_column_priv_only (a) VALUES (1);

** SHOW COLUMNS
** Should succeed because we have privileges (any) on at least one of the columns.
select column_name as 'Field',column_type as 'Type',is_nullable as 'Null',column_key as 'Key',column_default as 'Default',extra as 'Extra' from information_schema.columns where table_schema='mysqltest_db1' and table_name='t_column_priv_only';
Field	Type	Null	Key	Default	Extra
a	int	YES		NULL	
show columns from mysqltest_db1.t_column_priv_only;
Field	Type	Null	Key	Default	Extra
a	int	YES		NULL	
** SHOW COLUMNS
** Should fail because there are no privileges on any column combination.
show columns from mysqltest_db1.t_no_priv;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 't_no_priv'
** However, select from I_S.COLUMNS will succeed but not show anything:
select column_name as 'Field',column_type as 'Type',is_nullable as 'Null',column_key as 'Key',column_default as 'Default',extra as 'Extra' from information_schema.columns where table_schema='mysqltest_db1' and table_name='t_no_priv';
Field	Type	Null	Key	Default	Extra

** CREATE TABLE ... LIKE ... require SELECT privleges and will fail.
create table test.t_no_priv like mysqltest_db1.column_priv_only;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 'column_priv_only'

** Just to be sure... SELECT also fails.
select * from mysqltest_db1.t_column_priv_only;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 't_column_priv_only'

** SHOW CREATE TABLE ... require any privileges on all columns (the entire table).
** First we try and fail on a table with only one column privilege.
show create table mysqltest_db1.t_column_priv_only;
ERROR 42000: SHOW command denied to user 'mysqltest_u1'@'localhost' for table 't_column_priv_only'

** Now we do the same on a table with SELECT privileges.

** SHOW COLUMNS
** Success because we got some privileges on the table (SELECT_ACL)
show columns from mysqltest_db1.t_select_priv;
Field	Type	Null	Key	Default	Extra
a	int	YES		NULL	
b	int	YES		NULL	

** CREATE TABLE ... LIKE ... require SELECT privleges and will SUCCEED.
drop table if exists test.t_duplicated;
create table test.t_duplicated like mysqltest_db1.t_select_priv;
drop table test.t_duplicated;

** SHOW CREATE TABLE will succeed because we have a privilege on all columns in the table (table-level privilege).
show create table mysqltest_db1.t_select_priv;
Table	Create Table
t_select_priv	CREATE TABLE `t_select_priv` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci

** SHOW CREATE TABLE will fail if there is no grants at all:
show create table mysqltest_db1.t_no_priv;
ERROR 42000: SHOW command denied to user 'mysqltest_u1'@'localhost' for table 't_no_priv'

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
Table	Op	Msg_type	Msg_text
mysqltest_db1.t6	analyze	status	OK
use mysqltest_db1;
** Connect as restricted user mysqltest_u1.
** SELECT FROM INFORMATION_SCHEMA.STATISTICS will succeed because any privileges will do (authentication is enough).
SELECT * FROM INFORMATION_SCHEMA.STATISTICS WHERE table_name='t5';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	NON_UNIQUE	INDEX_SCHEMA	INDEX_NAME	SEQ_IN_INDEX	COLUMN_NAME	COLLATION	CARDINALITY	SUB_PART	PACKED	NULLABLE	INDEX_TYPE	COMMENT	INDEX_COMMENT	IS_VISIBLE	EXPRESSION
** SHOW INDEX FROM t5 will fail because we don't have any privileges on any column combination.
SHOW INDEX FROM t5;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 't5'
** SHOW INDEX FROM t6 will succeed because there exist a privilege on a column combination on t6.
SHOW INDEX FROM t6;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t6	1	i	1	s1	A	0	NULL	NULL	YES	BTREE			YES	NULL
** CHECK TABLE requires any privilege on any column combination and should succeed for t6:
CHECK TABLE t6;
Table	Op	Msg_type	Msg_text
mysqltest_db1.t6	check	status	OK
** With no privileges access is naturally denied:
CHECK TABLE t5;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 't5'
** CHECKSUM TABLE requires SELECT privileges on the table. The following should fail:
CHECKSUM TABLE t6;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 't6'
** And this should work:
CHECKSUM TABLE t_select_priv;
Table	Checksum
mysqltest_db1.t_select_priv	0
SHOW CREATE VIEW v5;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 'v5'
SHOW CREATE VIEW v6;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 'v6'
SHOW CREATE VIEW v2;
ERROR 42000: SELECT command denied to user 'mysqltest_u1'@'localhost' for table 'v2'
SHOW CREATE VIEW v3;
View	Create View	character_set_client	collation_connection
v3	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v3` AS select `t_select_priv`.`a` AS `a`,`t_select_priv`.`b` AS `b` from `t_select_priv`	utf8mb4	utf8mb4_0900_ai_ci
drop database mysqltest_db1;
drop user mysqltest_u1@localhost;
USE test;
#
# Bug #28653104: HANDLE_FATAL_SIGNAL (SIG=11) IN __STRCHR_SSE2
#
# Setup
call mtr.add_suppression("The plugin .* used to authenticate user .* is not loaded. Nobody can currently login using this account");
call mtr.add_suppression("User entry .* has an empty plugin value. The user will be ignored and no one can login with this user anymore.");
call mtr.add_suppression("Some of the user accounts with SUPER privileges were disabled because of empty mysql.user.plugin value.");
call mtr.add_suppression("Found an entry in the 'role_edges' table with unknown authorization ID '`r3`@`%`'");
CREATE TEMPORARY TABLE save_user AS SELECT * FROM mysql.user;
CREATE TEMPORARY TABLE save_role_edges AS SELECT * FROM mysql.role_edges;
# Test
INSERT INTO mysql.user(user,plugin,ssl_cipher,x509_issuer,x509_subject) VALUES ('foo','bar','','',''),('bar','bar','','',''),('baz','bar','','','');
CREATE ROLE r1,r2,r3;
GRANT r3 TO r2;
CREATE USER''@''IDENTIFIED WITH 'server' AS 'user';
ERROR HY000: Plugin 'server' is not loaded
UPDATE mysql.user SET plugin='';
# Success criteria: must not crash
CREATE USER plug IDENTIFIED WITH 'server';
ERROR HY000: Plugin 'server' is not loaded
# Cleanup
DELETE FROM mysql.user;
INSERT INTO mysql.user SELECT * FROM save_user;
DELETE FROM mysql.role_edges;
INSERT INTO mysql.role_edges SELECT * FROM save_role_edges;
FLUSH PRIVILEGES;
#
# Bug#29395197 REVOKE ALLOWS INCORRECT VALUES
#
CREATE USER testuser@localhost;
GRANT abc ON *.* TO testuser@localhost;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use
REVOKE abc ON *.* FROM testuser@localhost;
Warnings:
Warning	3929	Dynamic privilege 'ABC' is not registered with the server.
CREATE ROLE testrole;
GRANT abc ON *.* TO testrole;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use
REVOKE abc ON *.* FROM testrole;
Warnings:
Warning	3929	Dynamic privilege 'ABC' is not registered with the server.
DROP USER testuser@localhost;
DROP USER testrole;
#
# Anonymous user fail to properly aggregate EXECUTE privilege.
#
CREATE USER ''@'';
GRANT EXECUTE ON PROCEDURE sys.table_exists TO ''@'';
GRANT SELECT ON db1.* TO ''@'';
SHOW GRANTS FOR ''@'';
Grants for @
GRANT USAGE ON *.* TO ``@``
GRANT SELECT ON `db1`.* TO ``@``
GRANT EXECUTE ON PROCEDURE `sys`.`table_exists` TO ``@``
DROP USER ''@'';
# End of 8.0 tests
