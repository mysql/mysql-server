SET @start_partial_revokes = @@global.partial_revokes;
SET @@global.partial_revokes=ON;
SET @@global.partial_revokes = ON;
SELECT @@global.partial_revokes;
@@global.partial_revokes
1
-------------------------------------------------------------------------
# 1. GRANT AS sanity checks
-------------------------------------------------------------------------
CREATE USER foo, bar, baz;
CREATE DATABASE db1;
CREATE TABLE db1.t1(c1 int);
GRANT SELECT ON db1.* TO foo AS bar;
ERROR HY000: GRANT ... AS is currently supported only for global privileges.
GRANT SELECT ON db1.t1 TO foo AS bar;
ERROR HY000: GRANT ... AS is currently supported only for global privileges.
GRANT SELECT(c1) ON db1.t1 TO foo AS bar;
ERROR HY000: GRANT ... AS is currently supported only for global privileges.
GRANT PROXY ON foo TO bar AS foo;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'AS foo' at line 1
GRANT baz TO bar AS foo;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'AS foo' at line 1
GRANT SELECT ON *.* TO bar WITH ROLE foo;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ROLE foo' at line 1
GRANT SELECT ON *.* TO bar WITH ROLE NONE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ROLE NONE' at line 1
GRANT SELECT ON *.* TO bar WITH ROLE ALL;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ROLE ALL' at line 1
GRANT SELECT ON *.* TO bar WITH ROLE DEFAULT;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ROLE DEFAULT' at line 1
GRANT SELECT ON *.* TO foo AS hohoho;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT ON *.* TO foo AS bar WITH ROLE hohoho;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT ON *.* TO foo AS bar WITH ROLE baz, hohoho;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP USER foo, bar, baz;
-------------------------------------------------------------------------
# 2. Permission checks
-------------------------------------------------------------------------
CREATE USER foo, bar;
CREATE ROLE baz, qux;
CREATE DATABASE db1;
GRANT SELECT ON *.* TO foo AS bar WITH ROLE baz;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT ON *.* TO foo AS `root`@`localhost`;
GRANT SELECT ON *.* TO foo WITH GRANT OPTION;
REVOKE SELECT ON db1.* FROM foo;
GRANT SELECT ON *.* TO bar AS `root`@`localhost`;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SYSTEM_USER ON *.* TO foo;
GRANT SELECT ON *.* TO bar AS `root`@`localhost`;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
REVOKE ALL ON *.* FROM foo;
GRANT SYSTEM_USER ON *.* TO foo;
GRANT INSERT ON *.* TO foo WITH GRANT OPTION;
GRANT SELECT ON *.* TO baz WITH GRANT OPTION;
GRANT SELECT ON db1.* to qux;
GRANT baz, qux TO foo;
SET ROLE baz;
GRANT SELECT ON *.* TO bar AS `root`@`localhost`;
SET ROLE qux;
GRANT SELECT ON *.* TO bar;
ERROR 28000: Access denied for user 'foo'@'%' (using password: NO)
DROP DATABASE db1;
DROP ROLE baz, qux;
DROP USER foo, bar;
CREATE USER foo, bar, baz;
CREATE ROLE r1, r2, r3;
GRANT SELECT ON *.* TO foo WITH GRANT OPTION;
GRANT SYSTEM_USER ON *.* TO r2;
GRANT r2 TO r3;
GRANT r1, r3 TO bar;
GRANT SELECT ON *.* TO baz AS bar;
GRANT SELECT ON *.* TO baz AS bar WITH ROLE NONE;
GRANT SELECT ON *.* TO baz AS bar WITH ROLE r1;
GRANT SELECT ON *.* TO baz AS bar WITH ROLE r2;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT ON *.* TO baz AS bar WITH ROLE r1, r2;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT ON *.* TO baz AS bar WITH ROLE ALL;
SHOW GRANTS FOR baz;
Grants for baz@%
GRANT SELECT ON *.* TO `baz`@`%`
DROP ROLE r1, r2, r3;
DROP USER foo, bar, baz;
-------------------------------------------------------------------------
# 3. User test
-------------------------------------------------------------------------
CREATE USER foo, bar;
CREATE DATABASE db1;
GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER();
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT, INSERT ON *.* TO `bar`@`%`
REVOKE ALL ON *.* FROM bar;
GRANT SELECT ON *.* TO bar AS foo;
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT ON *.* TO `bar`@`%`
REVOKE ALL ON *.* FROM bar;
GRANT SELECT ON *.* TO foo;
REVOKE SELECT ON db1.* FROM foo;
GRANT SELECT ON *.* TO bar AS foo;
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT ON *.* TO `bar`@`%`
REVOKE SELECT ON `db1`.* FROM `bar`@`%`
REVOKE ALL ON *.* FROM bar;
GRANT SELECT, INSERT ON *.* TO bar AS foo;
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT, INSERT ON *.* TO `bar`@`%`
REVOKE SELECT ON `db1`.* FROM `bar`@`%`
DROP DATABASE db1;
DROP USER foo, bar;
-------------------------------------------------------------------------
# 4. Roles test
-------------------------------------------------------------------------
CREATE USER foo, bar;
CREATE ROLE r1, r2, r3, r4;
CREATE DATABASE db1;
GRANT SELECT ON *.* TO r1;
REVOKE SELECT ON db1.* FROM r1;
GRANT r1 TO r2;
GRANT INSERT ON *.* TO r3;
REVOKE INSERT ON db1.* FROM r3;
GRANT r3 TO r4;
GRANT r2, r4 TO foo;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL;
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT, INSERT ON *.* TO `bar`@`%`
REVOKE SELECT, INSERT ON `db1`.* FROM `bar`@`%`
REVOKE ALL ON *.* FROM bar;
ALTER USER foo DEFAULT ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL;
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT, INSERT ON *.* TO `bar`@`%`
REVOKE SELECT, INSERT ON `db1`.* FROM `bar`@`%`
REVOKE ALL ON *.* FROM bar;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL EXCEPT r4;
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT, INSERT ON *.* TO `bar`@`%`
REVOKE SELECT ON `db1`.* FROM `bar`@`%`
REVOKE ALL ON *.* FROM bar;
ALTER USER foo DEFAULT ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE r2, r4;
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT, INSERT ON *.* TO `bar`@`%`
REVOKE SELECT, INSERT ON `db1`.* FROM `bar`@`%`
DROP DATABASE db1;
DROP ROLE r1, r2, r3, r4;
DROP USER foo, bar;
-------------------------------------------------------------------------
# 6. Without --partial_revokes
-------------------------------------------------------------------------
SET @save_partial_revokes = @@global.partial_revokes;
SET @@global.partial_revokes = OFF;
CREATE USER foo, bar;
CREATE ROLE r1, r2;
GRANT r1, r2 TO foo;
ALTER USER foo DEFAULT ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS foo;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL EXCEPT r1;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE DEFAULT;
DROP USER foo, bar;
DROP ROLE r1, r2;
SET @@global.partial_revokes = @save_partial_revokes;
-------------------------------------------------------------------------
# 7. Binlog tests
-------------------------------------------------------------------------
CREATE USER foo, bar;
CREATE ROLE r1, r2, r3, r4;
CREATE DATABASE db1;
GRANT SELECT ON *.* TO r1 WITH GRANT OPTION;
REVOKE SELECT ON db1.* FROM r1;
GRANT r1 TO r2;
GRANT INSERT ON *.* TO r3 WITH GRANT OPTION;
REVOKE INSERT ON db1.* FROM r3;
GRANT r3 TO r4;
GRANT r2, r4 TO foo;
ALTER USER foo DEFAULT ROLE ALL;
include/save_binlog_position.inc
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE ALL EXCEPT r2;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE r2, r4;
GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER();
GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER() WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER() WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO bar AS CURRENT_USER() WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE ALL EXCEPT r2;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO foo AS foo WITH ROLE r2, r4;
GRANT SELECT, INSERT ON *.* TO bar;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'foo'@'%' WITH ROLE ALL
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'foo'@'%' WITH ROLE NONE
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'foo'@'%' WITH ROLE ALL EXCEPT 'r2'@'%'
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'foo'@'%' WITH ROLE DEFAULT
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'foo'@'%' WITH ROLE 'r2'@'%','r4'@'%'
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'root'@'localhost' WITH ROLE NONE
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'root'@'localhost' WITH ROLE ALL
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'root'@'localhost' WITH ROLE NONE
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'root'@'localhost' WITH ROLE DEFAULT
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'foo'@'%' AS 'foo'@'%' WITH ROLE ALL
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'foo'@'%' AS 'foo'@'%' WITH ROLE NONE
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'foo'@'%' AS 'foo'@'%' WITH ROLE ALL EXCEPT 'r2'@'%'
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'foo'@'%' AS 'foo'@'%' WITH ROLE DEFAULT
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'foo'@'%' AS 'foo'@'%' WITH ROLE 'r2'@'%','r4'@'%'
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'foo'@'%' WITH ROLE 'r2'@'%','r4'@'%'
GRANT SELECT, INSERT ON *.* TO foo;
REVOKE SELECT, INSERT ON db1.* FROM foo;
include/save_binlog_position.inc
GRANT SELECT, INSERT ON *.* TO bar AS foo;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	use `test`; GRANT SELECT, INSERT ON *.* TO 'bar'@'%' AS 'foo'@'%' WITH ROLE NONE
DROP DATABASE db1;
DROP ROLE r1, r2, r3, r4;
DROP USER foo, bar;
-------------------------------------------------------------------------
# 8. Current user in AS caluse
-------------------------------------------------------------------------
CREATE USER foo, bar, baz;
CREATE ROLE r1, r2;
GRANT SELECT, INSERT ON *.* TO r1 WITH GRANT OPTION;
REVOKE SELECT ON mysql.* FROM r1;
GRANT SELECT, INSERT ON *.* TO r2 WITH GRANT OPTION;
REVOKE INSERT ON mysql.* FROM r2;
GRANT r1, r2 TO foo;
ALTER USER foo DEFAULT ROLE ALL;
GRANT SELECT ON *.* TO bar AS current_user() WITH ROLE r1;
GRANT INSERT ON *.* TO baz AS foo WITH ROLE r2;
SHOW GRANTS FOR bar;
Grants for bar@%
GRANT SELECT ON *.* TO `bar`@`%`
REVOKE SELECT ON `mysql`.* FROM `bar`@`%`
SHOW GRANTS FOR baz;
Grants for baz@%
GRANT INSERT ON *.* TO `baz`@`%`
REVOKE INSERT ON `mysql`.* FROM `baz`@`%`
DROP ROLE r1, r2;
DROP USER foo, bar, baz;
CREATE USER foo, bar;
CREATE ROLE r1, r2;
GRANT r1, r2 TO foo;
GRANT SELECT ON *.* TO r1 WITH GRANT OPTION;
REVOKE SELECT ON mysql.* FROM r1;
GRANT INSERT ON *.* TO r2 WITH GRANT OPTION;
REVOKE INSERT ON mysql.* FROM r2;
ALTER USER foo DEFAULT ROLE ALL;
GRANT SELECT, INSERT ON *.* TO bar AS current_user();
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT, INSERT ON *.* TO bar AS current_user() WITH ROLE r1;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT, INSERT ON *.* TO bar AS current_user() WITH ROLE r2;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT, INSERT ON *.* TO bar AS foo;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE r1;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
GRANT SELECT, INSERT ON *.* TO bar AS foo WITH ROLE r2;
ERROR HY000: Either some of the authorization IDs in the AS clause are invalid or the current user lacks privileges to execute the statement.
DROP ROLE r1, r2;
DROP USER foo, bar;
-------------------------------------------------------------------------
# 9. General log
-------------------------------------------------------------------------
SET @save_partial_revokes = @@global.partial_revokes;
CREATE USER u1, u2;
CREATE ROLE r1, r2;
GRANT r1, r2 TO u1;
GRANT SELECT ON *.* TO r1 WITH GRANT OPTION;
GRANT INSERT ON *.* TO r2 WITH GRANT OPTION;
ALTER USER u1 DEFAULT ROLE ALL;
TRUNCATE TABLE mysql.general_log;
SET @old_log_output= @@global.log_output;
SET @old_general_log= @@global.general_log;
SET @old_general_log_file= @@global.general_log_file;
SET GLOBAL general_log_file = 'MYSQLTEST_VARDIR/log/grant_as_ddl_general.log';
SET GLOBAL log_output = 'FILE,TABLE';
SET GLOBAL general_log= 'ON';
SET @@global.partial_revokes = ON;
REVOKE SELECT ON mysql.* FROM r1;
REVOKE INSERT ON mysql.* FROM r2;
GRANT SELECT, INSERT ON *.* TO u2;
GRANT SELECT, INSERT ON *.* TO u2 AS u1;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r1;
SET ROLE NONE;
SET ROLE r1;
GRANT SELECT ON *.* TO u2;
SET ROLE r1, r2;
GRANT SELECT, INSERT ON *.* TO u2;
SET ROLE ALL;
GRANT SELECT, INSERT ON *.* TO u2;
SET ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO u2;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE r1;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE ALL;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE DEFAULT;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r2;
SET ROLE NONE;
GRANT SELECT ON *.* TO r1;
GRANT INSERT ON *.* TO r2;
SET @@global.partial_revokes = OFF;
GRANT SELECT, INSERT ON *.* TO u2;
GRANT SELECT, INSERT ON *.* TO u2 AS u1;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE NONE;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL;
GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r1;
SET ROLE NONE;
SET ROLE r1;
GRANT SELECT ON *.* TO u2;
SET ROLE r1, r2;
GRANT SELECT, INSERT ON *.* TO u2;
SET ROLE ALL;
GRANT SELECT, INSERT ON *.* TO u2;
SET ROLE DEFAULT;
GRANT SELECT, INSERT ON *.* TO u2;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE r1;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE ALL;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE DEFAULT;
GRANT SELECT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r2;
SET ROLE NONE;
DROP ROLE r1, r2;
DROP USER u1, u2;
SET @@global.partial_revokes = @save_partial_revokes;
Show what is logged:
------ rewrite ------
SELECT COUNT(*) = 8 AS NO_AS_CLAUSE_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2' AND command_type NOT LIKE 'Prepare';
NO_AS_CLAUSE_COUNT
1
SELECT COUNT(*) = 2 AS WITH_AS_WITHOUT_ROLE_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1' AND command_type NOT LIKE 'Prepare';
WITH_AS_WITHOUT_ROLE_COUNT
1
SELECT COUNT(*) = 2 AS WITH_AS_WITH_ROLE_NONE_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE NONE' AND command_type NOT LIKE 'Prepare';
WITH_AS_WITH_ROLE_NONE_COUNT
1
SELECT COUNT(*) = 2 AS WITH_AS_WITH_ROLE_DEFAULT_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE DEFAULT' AND command_type NOT LIKE 'Prepare';
WITH_AS_WITH_ROLE_DEFAULT_COUNT
1
SELECT COUNT(*) = 2 AS WITH_AS_WITH_ROLE_ALL_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL' AND command_type NOT LIKE 'Prepare';
WITH_AS_WITH_ROLE_ALL_COUNT
1
SELECT COUNT(*) = 2 AS WITH_AS_WITH_ROLE_ALL_EXCEPT_COUNT FROM mysql.general_log WHERE argument LIKE 'GRANT SELECT, INSERT ON *.* TO u2 AS u1 WITH ROLE ALL EXCEPT r1' AND command_type NOT LIKE 'Prepare';
WITH_AS_WITH_ROLE_ALL_EXCEPT_COUNT
1
------ done ------
SET GLOBAL general_log_file= @old_general_log_file;
SET GLOBAL general_log= @old_general_log;
SET GLOBAL log_output= @old_log_output;
TRUNCATE TABLE mysql.general_log;
-------------------------------------------------------------------------
SET @@global.partial_revokes = @start_partial_revokes;
